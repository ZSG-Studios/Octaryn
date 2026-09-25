"""Convert source map textures (DDS) to engine-supported PNG/JPEG.

The client map loader decodes PNG/JPEG only (DDS/KTX2/WebP are rejected),
so bundled GLB maps need raster image inputs. Opaque textures become JPEG,
alpha-bearing textures keep PNG. Packed ORM and normals remain lossless PNG.
Bistro BC5 normals reconstruct positive Z and invert DirectX Y for glTF.
Output is capped to --max-size pixels.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import sys
import struct
from pathlib import Path

from PIL import Image, ImageMath, ImageOps

Image.MAX_IMAGE_PIXELS = None


def normal_rgb(source: Path, image: Image.Image) -> Image.Image:
    """Decode BC5's two stored components and convert DirectX tangent Y to glTF."""
    channels = list(image.convert('RGB').split())
    with source.open('rb') as stream:
        header = stream.read(148)
    fourcc = header[84:88]
    bc5 = fourcc in (b'ATI2', b'BC5U', b'BC5S') or (
        fourcc == b'DX10' and len(header) >= 132 and struct.unpack_from('<I', header, 128)[0] in (83, 84))
    if bc5:
        def reconstruct(args):
            x = args['r'] * (2.0 / 255.0) - 1.0
            y = args['g'] * (2.0 / 255.0) - 1.0
            squared = 1.0 - x * x - y * y
            return ((squared * (squared > 0)) ** .5 * .5 + .5) * 255 + .5
        channels[2] = ImageMath.lambda_eval(reconstruct, r=channels[0].convert('F'), g=channels[1].convert('F')).convert('L')
    channels[1] = ImageOps.invert(channels[1])
    return Image.merge('RGB', channels)


def resize_normal(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    """Average unit vectors with nonnegative BOX weights, then renormalize."""
    channels = [ImageMath.lambda_eval(lambda a: a['c'] * (2.0 / 255.0) - 1.0,
                                     c=c.convert('F')) for c in image.convert('RGB').split()]

    def normalize(values):
        squared = ImageMath.lambda_eval(lambda a: a['x'] ** 2 + a['y'] ** 2 + a['z'] ** 2,
                                        x=values[0], y=values[1], z=values[2])
        length = ImageMath.lambda_eval(lambda a: (a['s'] + (a['s'] < 1e-12) * 1e-12) ** .5,
                                       s=squared)
        result = [ImageMath.lambda_eval(lambda a: a['c'] / a['n'], c=c, n=length) for c in values]
        result[2] = ImageMath.lambda_eval(lambda a: a['z'] + (a['s'] < 1e-12),
                                         z=result[2], s=squared)
        return result

    channels = normalize(channels)
    if image.size != size:
        channels = [c.resize(size, Image.Resampling.BOX) for c in channels]
        channels = normalize(channels)
    encoded = [ImageMath.lambda_eval(lambda a: (a['c'] * .5 + .5) * 255 + .5,
                                    c=c).convert('L') for c in channels]
    return Image.merge('RGB', encoded)


def convert_one(source: Path, target_dir: Path, max_size: int) -> tuple[str, int]:
    """Convert one DDS to PNG (alpha) or JPEG (opaque). Returns (status, bytes)."""
    with Image.open(source) as image:
        image.load()
        normal = source.stem.lower().endswith('_normal')
        data_map = normal or source.stem.lower().endswith('_specular')
        if image.mode in ("RGBA", "LA", "PA") or (
            image.mode == "P" and "transparency" in image.info
        ):
            converted = image.convert("RGBA")
            # BC1 sources decode as RGBA with a fully-opaque alpha channel;
            # only keep the alpha path when the source really uses it.
            alpha_min = converted.getchannel("A").getextrema()[0]
            if alpha_min >= 250:
                converted = converted.convert("RGB")
        else:
            converted = image.convert("RGB")
        if normal:
            converted = normal_rgb(source, converted)
        size = converted.size
        if max(converted.size) > max_size:
            scale = max_size / max(converted.size)
            size = (
                max(1, round(converted.width * scale)),
                max(1, round(converted.height * scale)),
            )
        if normal:
            converted = resize_normal(converted, size)
        elif converted.size != size:
            converted = converted.resize(size, Image.LANCZOS)
        stem = source.stem + ('_GL' if normal else '')
        if converted.mode == "RGBA" or data_map:
            target = target_dir / (stem + ".png")
            converted.save(target, optimize=True)
        else:
            target = target_dir / (stem + ".jpg")
            converted.save(target, quality=88, optimize=True)
    return ("ok", target.stat().st_size)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--target-dir", required=True, type=Path)
    parser.add_argument(
        "--patterns",
        nargs="+",
        default=["*BaseColor.dds", "*Emissive.dds", "*Specular.dds", "*Normal.dds"],
        help="Filename patterns to convert",
    )
    parser.add_argument("--max-size", type=int, default=1024)
    parser.add_argument('--overwrite', action='store_true', help='Regenerate matching converted images')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()

    args.target_dir.mkdir(parents=True, exist_ok=True)
    sources: list[Path] = []
    for pattern in args.patterns:
        sources.extend(sorted(args.source_dir.glob(pattern)))

    failures: list[str] = []
    total_bytes = 0
    converted_count = 0
    pending = []
    for source in sources:
        target_stem = source.stem + ('_GL' if source.stem.lower().endswith('_normal') else '')
        data_map = source.stem.lower().endswith(('_normal', '_specular'))
        existing = [
            args.target_dir / (target_stem + ext) for ext in ((".png",) if data_map else (".png", ".jpg"))
        ]
        if not args.overwrite and any(path.exists() for path in existing):
            continue
        pending.append(source)
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as workers:
        jobs = {workers.submit(convert_one, source, args.target_dir, args.max_size): source for source in pending}
        for job in as_completed(jobs):
            try:
                status, size = job.result()
                total_bytes += size
                converted_count += 1
            except Exception as error:  # noqa: BLE001 - report and continue batch
                failures.append(f"{jobs[job].name}: {error}")

    print(f"converted={converted_count} total_mb={total_bytes / 1e6:.1f}")
    for failure in failures:
        print(f"FAIL {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
