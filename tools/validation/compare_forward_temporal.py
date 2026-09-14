"""Compare captured production cloud/water sequences, including fixed reference edge pixels."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics
import struct
import sys


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def image(path):
    data = path.read_bytes()
    require(data[:2] == b"BM", f"Not a BMP: {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    require(width > 0 and height and bits in (24, 32) and compression in (0, 3), "Unsupported BMP layout")
    if compression == 3:
        require(struct.unpack_from("<III", data, 54) == (0xFF0000, 0xFF00, 0xFF), "Unsupported BMP channel masks")
    stride = ((width*bits+31)//32)*4
    require(len(data) >= offset+stride*abs(height), "Truncated captured BMP")
    pixels = bytearray(width*abs(height)*3)
    for y in range(abs(height)):
        source = offset+(y if height < 0 else abs(height)-1-y)*stride
        for x in range(width):
            p, q = source+x*(bits//8), (y*width+x)*3
            pixels[q:q+3] = data[p:p+3][::-1]
    return width, abs(height), pixels, hashlib.sha256(data).hexdigest()


def edges(pixels, width, height):
    selected = []
    for y in range(1, height-1):
        for x in range(1, width-1):
            p = (y*width+x)*3
            if max(abs(pixels[p+c]-pixels[p+offset+c]) for offset in (-3, 3, -width*3, width*3)
                   for c in range(3)) > 8:
                selected.append(y*width+x)
    require(len(selected) >= 16, "Reference lacks meaningful cloud/water edges")
    return selected


def delta(a, b, selected):
    values = [sum(abs(a[p*3+c]-b[p*3+c]) for c in range(3))/(3*255) for p in selected]
    values.sort()
    return statistics.fmean(values), values[math.ceil(len(values)*.99)-1], values[-1]


def read_rows(folder):
    with (folder / "frames.csv").open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    require(len(rows) == 32 and all(int(row["frame"]) == 64+i for i, row in enumerate(rows)),
            "Sequence must contain 32 contiguous post-warmup frames")
    require([row["phase"] for row in rows] == ["stationary"]*16+["moving"]*16,
            "Unexpected camera sequence phases")
    require(all(int(row["reset"]) == 0 for row in rows), "Sequence repeatedly resets temporal history")
    require(max(int(row["foreground_pixels"]) for row in rows) >= 256, "Layer coverage is insufficient")
    return rows


def sequence(folder, reference):
    rows = read_rows(folder)
    images = [image(folder / f"frame-{frame:02}.bmp") for frame in range(32)]
    require(all(item[:2] == reference[:2] for item in images), "Output sizes differ")
    width, height = reference[:2]
    selected = edges(reference[2], width, height)
    phases = {}
    for name, indices in (("stationary", range(1, 16)), ("moving", range(16, 32))):
        full = [float(rows[i]["resolved_mean_delta"]) for i in indices]
        edge = [delta(images[i][2], images[i-1][2], selected) for i in indices]
        phases[name] = {"pairs": len(full), "full_mean_delta": statistics.fmean(full),
                        "edge_mean_delta": statistics.fmean(value[0] for value in edge),
                        "edge_p99_delta_mean": statistics.fmean(value[1] for value in edge),
                        "edge_max_delta": max(value[2] for value in edge),
                        "raw_mean_delta": statistics.fmean(float(rows[i]["raw_mean_delta"]) for i in indices)}
    return {"size": [width, height], "reference_edge_pixels": len(selected), "phases": phases,
            "frame_sha256": [item[3] for item in images],
            "camera": [[row[key] for key in ("camera_x", "camera_y", "camera_z")] for row in rows]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = {"status": "passed", "before": str(args.before.resolve()), "after": str(args.after.resolve()),
              "cases": {}, "limits": "Stationary differences measure jitter shimmer with frozen scene time. Moving differences include real parallax; their magnitude alone does not prove improved visual quality. Edge masks use the same Off reference for both versions."}
    for layer in ("clouds", "water"):
        reference = image(args.before / f"{layer}-off/frame-00.bmp")
        before_off = sequence(args.before / f"{layer}-off", reference)
        after_off = sequence(args.after / f"{layer}-off", reference)
        require(before_off["frame_sha256"][:16] == after_off["frame_sha256"][:16],
                f"{layer} stationary Off reference changed between versions")
        moving_differences = []
        for frame in range(16, 32):
            if before_off["frame_sha256"][frame] == after_off["frame_sha256"][frame]:
                continue
            old = image(args.before / f"{layer}-off/frame-{frame:02}.bmp")[2]
            new = image(args.after / f"{layer}-off/frame-{frame:02}.bmp")[2]
            errors = [abs(a-b) for a, b in zip(old, new)]
            moving_differences.append({"frame": frame, "changed_rgb_channels": sum(value != 0 for value in errors),
                                       "maximum_absolute_rgb_difference": max(errors)/255,
                                       "mean_absolute_rgb_difference": statistics.fmean(errors)/255})
        for mode in ("off", "native", "quality"):
            name = f"{layer}-{mode}"
            before = before_off if mode == "off" else sequence(args.before/name, reference)
            after = after_off if mode == "off" else sequence(args.after/name, reference)
            require(before["camera"] == after["camera"], "Before/after camera paths differ")
            improvement = {}
            for metric in ("full_mean_delta", "edge_mean_delta", "edge_p99_delta_mean"):
                old, new = (run["phases"]["stationary"][metric] for run in (before, after))
                improvement[metric] = {"before": old, "after": new,
                                       "reduction_percent": 100*(1-new/old) if old else None}
            report["cases"][name] = {"before": before, "after": after, "stationary_comparison": improvement}
            if mode == "off":
                report["cases"][name]["moving_reference_differences"] = moving_differences
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"forward_temporal_comparison=passed evidence={args.output.resolve()}")
    for name, case in report["cases"].items():
        print(name, json.dumps(case["stationary_comparison"]))


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError, struct.error) as error:
        print(f"forward_temporal_comparison=failed: {error}", file=sys.stderr)
        sys.exit(1)
