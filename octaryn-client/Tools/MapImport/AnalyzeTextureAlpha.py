"""Classify converted alpha textures as cutout (MASK) or true transparency (BLEND).

Cutout textures (foliage, fences) are almost fully opaque or fully transparent;
glass and fabric carry intermediate alpha values. The Blender import script uses
this classification to build glTF alphaMode MASK vs BLEND materials.
"""

import argparse
import json
from pathlib import Path

from PIL import Image

Image.MAX_IMAGE_PIXELS = None

ENDPOINT_TOLERANCE = 12
INTERMEDIATE_FRACTION_CAP = 0.10


def classify(image: Image.Image) -> str | None:
    """Return MASK/BLEND for alpha textures, None when effectively opaque."""
    alpha = image.getchannel("A")
    if alpha.getextrema()[0] == 255:
        return None
    histogram = alpha.histogram()
    total = sum(histogram)
    if total == 0:
        return None
    transparent = sum(histogram[:ENDPOINT_TOLERANCE + 1])
    opaque = sum(histogram[255 - ENDPOINT_TOLERANCE:])
    intermediate = total - transparent - opaque
    # Require both endpoints: uniformly transparent glass is not a cutout.
    if transparent and opaque and intermediate / total <= INTERMEDIATE_FRACTION_CAP:
        return "MASK"
    return "BLEND"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--texture-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    modes: dict[str, str] = {}
    for source in sorted(args.texture_dir.iterdir()):
        if source.suffix.lower() not in (".png", ".jpg", ".jpeg"):
            continue
        with Image.open(source) as image:
            image.load()
            modes[source.stem] = classify(image.convert("RGBA")) or "OPAQUE"

    args.output.write_text(json.dumps(modes, indent=2))
    blend = sum(1 for mode in modes.values() if mode == "BLEND")
    mask = sum(1 for mode in modes.values() if mode == "MASK")
    opaque = len(modes) - blend - mask
    print(f"textures={len(modes)} opaque={opaque} mask={mask} blend={blend}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
