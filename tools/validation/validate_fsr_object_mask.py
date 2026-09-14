"""Check the captured opaque-avatar FSR mask; this fixture excludes transparent overlays."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def inspect(capture):
    capture = capture.resolve()
    metadata_path = Path(str(capture) + ".temporal.json")
    metadata_bytes = metadata_path.read_bytes()
    metadata = json.loads(metadata_bytes)
    width, height = metadata["render_width"], metadata["render_height"]
    require(isinstance(width, int) and isinstance(height, int) and width > 0 and height > 0,
            "Invalid active render extent")
    require(metadata["mode"] > 0 and metadata["reset"] == 0,
            "Capture must exercise enabled FSR with accumulated history")
    require(metadata["scene_domain"] == "tone_mapped_linear" and metadata["opaque_domain"] == "hdr_linear",
            "Capture color domains differ from the production mask contract")
    images = {image["name"]: image for image in metadata["images"]}
    require(len(images) == len(metadata["images"]), "Duplicate capture image names")
    formats = {"object": ("RGBA16Float", 8), "reactive": ("R8Unorm", 1),
               "opaque": ("RGBA16Float", 8), "scene": ("RGBA16Float", 8)}
    blobs, hashes = {}, {}
    for name, (format_name, stride) in formats.items():
        image = images[name]
        require(image["format"] == format_name and image["col_pitch"] == stride,
                f"Unexpected {name} capture format")
        require(image["width"] >= width and image["height"] >= height and
                image["row_pitch"] >= image["width"] * stride,
                f"Invalid {name} extent or row pitch")
        blob = Path(str(capture) + f".{name}.bin").read_bytes()
        require(len(blob) == image["bytes"] and len(blob) >= image["row_pitch"] * image["height"],
                f"Truncated {name} capture")
        blobs[name] = blob
        hashes[name] = hashlib.sha256(blob).hexdigest()

    def pixel(name, x, y):
        image = images[name]
        offset = y * image["row_pitch"] + x * image["col_pitch"]
        values = struct.unpack_from("<4e", blobs[name], offset)
        require(all(math.isfinite(value) for value in values), f"Nonfinite {name} pixel at {x},{y}")
        return values

    count, total, minimum, maximum, high, zero = 0, 0, 255, 0, 0, 0
    difference_total, difference_maximum = 0.0, 0.0
    for y in range(height):
        for x in range(width):
            object_pixel = pixel("object", x, y)
            require(object_pixel[2] in (0, 1), f"Invalid object coverage at {x},{y}")
            if object_pixel[2] == 0:
                continue
            image = images["reactive"]
            value = blobs["reactive"][y * image["row_pitch"] + x * image["col_pitch"]]
            opaque, scene = pixel("opaque", x, y), pixel("scene", x, y)
            difference = max(abs(scene[c] - max(opaque[c], 0) / (1 + max(opaque[c], 0))) for c in range(3))
            count += 1
            total += value
            minimum, maximum = min(minimum, value), max(maximum, value)
            high += value > 127
            zero += value == 0
            difference_total += difference
            difference_maximum = max(difference_maximum, difference)
    require(count >= 256 and count / (width * height) >= .001,
            "Capture lacks enough opaque-avatar coverage to qualify its mask")
    return {"capture": str(capture), "render": [width, height], "mode": metadata["mode"],
            "dynamic_active": metadata["dynamic_active"], "object_pixels": count,
            "object_coverage": count / (width * height), "reactive_mean": total / count / 255,
            "reactive_min": minimum / 255, "reactive_max": maximum / 255,
            "reactive_above_half": high, "reactive_zero": zero,
            "opaque_scene_difference_mean": difference_total / count,
            "opaque_scene_difference_max": difference_maximum,
            "metadata_sha256": hashlib.sha256(metadata_bytes).hexdigest(), "raw_sha256": hashes}


def validate(capture, baseline=None):
    result = inspect(capture)
    require(result["reactive_max"] <= 1 / 255,
            "Opaque-avatar pixels remain reactive; transparent overlays are excluded from this fixture")
    require(result["opaque_scene_difference_max"] <= .002,
            "Opaque snapshot omits visible avatar color or the fixture includes transparent overlays")
    report = {"status": "passed", "capture": result}
    if baseline is not None:
        before = inspect(baseline)
        require((before["render"], before["mode"], before["dynamic_active"]) ==
                (result["render"], result["mode"], result["dynamic_active"]),
                "Before/after active extents or FSR modes differ")
        require(.8 <= result["object_pixels"] / before["object_pixels"] <= 1.2,
                "Before/after avatar coverage differs materially")
        require(before["reactive_mean"] > .5 and before["reactive_above_half"] / before["object_pixels"] >= .9,
                "Baseline does not reproduce the opaque-avatar reactivity defect")
        require(before["opaque_scene_difference_mean"] > .05,
                "Baseline does not demonstrate the missing opaque-avatar snapshot")
        report.update(baseline=before,
                      reactive_mean_reduction=before["reactive_mean"] - result["reactive_mean"])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", required=True, type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = args.output or Path(str(args.capture) + ".object-reactivity.json")
    try:
        report = validate(args.capture, args.baseline)
    except (OSError, ValueError, KeyError, TypeError, RuntimeError, struct.error) as error:
        report = {"status": "failed", "capture": str(args.capture.resolve()), "error": str(error)}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"fsr_object_mask={report['status']} evidence={output.resolve()}")
    if report["status"] != "passed":
        print(report["error"], file=sys.stderr)
        return 1
    print(f"object_pixels={report['capture']['object_pixels']} reactive_mean={report['capture']['reactive_mean']:.8f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
