#!/usr/bin/env python3
import argparse
import hashlib
import io
import json
import math
import sys
import urllib.request
import zipfile
from pathlib import Path

from PIL import Image, ImageEnhance


DEFAULT_PACK_URL = (
    "https://github.com/ClassicFaithful/Classic-32x-Jappa-Java/archive/refs/heads/"
    "java-latest.zip"
)
DEFAULT_PACK_FILE_NAME = "Classic-32x-Jappa-Java-java-latest.zip"
DEFAULT_PACK_NAME = "Classic Faithful 32x Jappa"
DEFAULT_PACK_SOURCE_URL = "https://github.com/ClassicFaithful/Classic-32x-Jappa-Java"
DEFAULT_PACK_LICENSE_NAME = "Faithful License Version 3"
DEFAULT_PACK_LICENSE_URL = "https://faithfulpack.net/license"
DEFAULT_PACK_CREDIT = "Faithful Resource Pack and ClassicFaithful contributors"
DEFAULT_PACK_CREDITS_URL = (
    "https://github.com/ClassicFaithful/Classic-32x-Jappa-Java/blob/main/credits.txt"
)
DEFAULT_PACK_LICENSE_FILE = "license.txt inside the source pack"
FALLBACK_PACK_NAME = "Octaryn fallback atlas"
FALLBACK_PACK_SOURCE_URL = "octaryn-basegame/Tools/build_atlas_from_pack.py --fallback-only"
FALLBACK_PACK_LICENSE_NAME = "Octaryn project source license"
FALLBACK_PACK_LICENSE_URL = "docs/THIRD_PARTY_NOTICES.md"
FALLBACK_PACK_CREDIT = "Octaryn generated fallback colors"
TEXTURE_PACK_LICENSE_NOTICE = (
    "Generated atlases may contain third-party texture pack work. Verify distribution "
    "rights, preserve required credit, and include the upstream license file before release."
)


ATLAS_SOURCES = {
    1: ("grass_block_top", ["grass_block_top.png"]),
    2: ("grass_block_side", ["grass_block_side.png"]),
    3: ("dirt", ["dirt.png"]),
    4: ("stone", ["stone.png"]),
    5: ("sand", ["sand.png"]),
    6: ("snow", ["snow.png", "snow_block.png"]),
    7: ("oak_log_top", ["oak_log_top.png"]),
    8: ("oak_log", ["oak_log.png", "oak_log_side.png"]),
    10: ("oak_leaves", ["oak_leaves.png"]),
    11: ("rose", ["poppy.png", "rose_bush_top.png", "red_tulip.png"]),
    12: ("gardenia", ["oxeye_daisy.png", "white_tulip.png"]),
    13: ("bluebell", ["cornflower.png", "blue_orchid.png"]),
    14: ("lavender", ["allium.png", "lilac_top.png"]),
    15: ("bush", ["short_grass.png", "grass.png", "fern.png"]),
    16: ("water", ["water_still.png"]),
    17: ("red_torch", ["redstone_torch.png", "torch.png"]),
    18: ("green_torch", ["torch.png"]),
    19: ("blue_torch", ["torch.png", "soul_torch.png"]),
    20: ("yellow_torch", ["torch.png"]),
    21: ("cyan_torch", ["soul_torch.png", "torch.png"]),
    22: ("magenta_torch", ["torch.png"]),
    23: ("white_torch", ["torch.png"]),
    24: ("oak_planks", ["oak_planks.png"]),
    25: ("glass", ["glass.png"]),
    26: ("water_flow", ["water_flow.png"]),
    27: ("lava_still", ["lava_still.png"]),
    28: ("lava_flow", ["lava_flow.png"]),
}

SOLID_FALLBACKS = {
    1: (106, 151, 65, 255),
    2: (111, 141, 68, 255),
    3: (134, 96, 67, 255),
    4: (125, 125, 125, 255),
    5: (218, 210, 158, 255),
    6: (240, 247, 247, 255),
    7: (151, 112, 63, 255),
    8: (103, 81, 49, 255),
    9: (255, 255, 255, 160),
    10: (73, 123, 38, 210),
    16: (64, 96, 255, 155),
    24: (162, 130, 78, 255),
    25: (190, 230, 240, 96),
    26: (64, 96, 255, 155),
    27: (255, 96, 0, 255),
    28: (255, 96, 0, 255),
}

SPRITE_FALLBACKS = {
    11: (196, 42, 42, 255),
    12: (245, 245, 225, 255),
    13: (72, 116, 214, 255),
    14: (154, 92, 202, 255),
    15: (92, 151, 54, 255),
    17: (236, 39, 63, 255),
    18: (90, 181, 82, 255),
    19: (51, 136, 222, 255),
    20: (243, 168, 51, 255),
    21: (54, 197, 244, 255),
    22: (250, 110, 121, 255),
    23: (255, 255, 255, 255),
}

TORCH_TINTS = {
    17: (1.45, 0.55, 0.55),
    18: (0.65, 1.25, 0.65),
    19: (0.55, 0.80, 1.45),
    20: (1.35, 1.05, 0.55),
    21: (0.55, 1.20, 1.35),
    22: (1.35, 0.65, 1.15),
    23: (1.25, 1.25, 1.25),
}

GRASS_BIOME_TINT = (0.57, 0.74, 0.35)
OAK_LEAVES_TINT = (0.72, 0.86, 0.58)
BUSH_TINT = (0.70, 0.84, 0.52)


def parse_args():
    parser = argparse.ArgumentParser(description="Build the Octaryn atlas from a Minecraft resource pack.")
    parser.add_argument("--url", default=DEFAULT_PACK_URL)
    parser.add_argument("--cache-dir", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--normal-output", default="")
    parser.add_argument("--specular-output", default="")
    parser.add_argument("--animation-output", default="")
    parser.add_argument("--animation-manifest", default="")
    parser.add_argument("--tile-size", type=int, required=True)
    parser.add_argument("--layer-count", type=int, required=True)
    parser.add_argument("--pack", default="")
    parser.add_argument("--sha256", default="")
    parser.add_argument("--pack-name", default="")
    parser.add_argument("--pack-source-url", default="")
    parser.add_argument("--pack-license-name", default="")
    parser.add_argument("--pack-license-url", default="")
    parser.add_argument("--pack-license-file", default="")
    parser.add_argument("--pack-credit", default="")
    parser.add_argument("--pack-credits-url", default="")
    parser.add_argument(
        "--fallback-only",
        action="store_true",
        help="Generate Octaryn-owned fallback atlas assets without reading a texture pack.",
    )
    return parser.parse_args()


def download_pack(url, cache_dir):
    cache_dir.mkdir(parents=True, exist_ok=True)
    destination = cache_dir / DEFAULT_PACK_FILE_NAME
    if destination.exists() and destination.stat().st_size > 0:
        return destination

    temporary = destination.with_suffix(destination.suffix + ".part")
    request = urllib.request.Request(url, headers={"User-Agent": "octaryn-basegame-atlas-pipeline/1.0"})
    with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as file:
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            file.write(chunk)
    temporary.replace(destination)
    return destination


def verify_sha256(path, expected):
    if not expected:
        return
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    actual = digest.hexdigest()
    if actual.lower() != expected.lower():
        raise RuntimeError(f"SHA256 mismatch for {path}: expected {expected}, got {actual}")


def zip_read_texture(zip_file, path):
    try:
        return zip_file.read(path)
    except KeyError:
        suffix = "/" + path
        for candidate in zip_file.namelist():
            if candidate.endswith(suffix):
                return zip_file.read(candidate)
    raise KeyError(path)


def zip_read_optional(zip_file, path):
    try:
        return zip_read_texture(zip_file, path)
    except KeyError:
        return None


def find_texture_entry(zip_file, names, suffix=""):
    if zip_file is None:
        return None, ""

    prefixes = (
        "assets/minecraft/textures/block/",
        "assets/minecraft/textures/blocks/",
    )
    for name in names:
        for prefix in prefixes:
            path = prefix + name.replace(".png", f"{suffix}.png")
            try:
                data = zip_read_texture(zip_file, path)
                return Image.open(io.BytesIO(data)).convert("RGBA"), path
            except KeyError:
                continue
    return None, ""


def find_texture(zip_file, names, suffix=""):
    image, _ = find_texture_entry(zip_file, names, suffix)
    return image


def make_solid_tile(color, tile_size):
    return Image.new("RGBA", (tile_size, tile_size), color)


def make_sprite_tile(color, tile_size):
    image = Image.new("RGBA", (tile_size, tile_size), (0, 0, 0, 0))
    pixels = image.load()
    cx = tile_size // 2
    for y in range(2, tile_size - 1):
        pixels[cx, y] = color
        if y > tile_size // 2 and cx + 1 < tile_size:
            pixels[cx + 1, y] = color
    for x in range(cx - 3, cx + 4):
        for y in range(3, 9):
            if 0 <= x < tile_size and abs(x - cx) + abs(y - 6) <= 4:
                pixels[x, y] = color
    return image


def clamp_byte(value):
    return max(0, min(255, round(value)))


def water_wave_height(x, y, tile_size):
    u = x / tile_size
    v = y / tile_size
    tau = math.tau
    wave = 0.0
    wave += 0.34 * math.sin(tau * (2.0 * u + 1.0 * v))
    wave += 0.24 * math.sin(tau * (-3.0 * u + 2.0 * v + 0.19))
    wave += 0.18 * math.cos(tau * (5.0 * u + 3.0 * v + 0.37))
    wave += 0.12 * math.sin(tau * (8.0 * u - 5.0 * v + 0.61))
    caustic = math.sin(tau * (4.0 * u + 4.0 * v)) * math.sin(tau * (4.0 * u - 4.0 * v))
    return max(0.0, min(1.0, 0.5 + wave * 0.38 + caustic * 0.08))


def build_water_tile(tile_size):
    image = Image.new("RGBA", (tile_size, tile_size), (0, 0, 0, 0))
    pixels = image.load()
    for y in range(tile_size):
        for x in range(tile_size):
            h = water_wave_height(x + 0.5, y + 0.5, tile_size)
            deep = (35, 76, 176)
            bright = (83, 139, 226)
            foam = max(0.0, h - 0.72) / 0.28
            red = deep[0] * (1.0 - h) + bright[0] * h + 18.0 * foam
            green = deep[1] * (1.0 - h) + bright[1] * h + 26.0 * foam
            blue = deep[2] * (1.0 - h) + bright[2] * h + 20.0 * foam
            alpha = 132.0 + h * 36.0
            pixels[x, y] = (clamp_byte(red), clamp_byte(green), clamp_byte(blue), clamp_byte(alpha))
    return image


def build_water_normal_tile(tile_size):
    image = Image.new("RGBA", (tile_size, tile_size), (128, 128, 255, 255))
    pixels = image.load()
    strength = 3.2
    for y in range(tile_size):
        for x in range(tile_size):
            left = water_wave_height((x - 1) % tile_size + 0.5, y + 0.5, tile_size)
            right = water_wave_height((x + 1) % tile_size + 0.5, y + 0.5, tile_size)
            down = water_wave_height(x + 0.5, (y - 1) % tile_size + 0.5, tile_size)
            up = water_wave_height(x + 0.5, (y + 1) % tile_size + 0.5, tile_size)
            dx = (right - left) * strength
            dy = (up - down) * strength
            nz = 1.0 / math.sqrt(dx * dx + dy * dy + 1.0)
            nx = -dx * nz
            ny = -dy * nz
            h = water_wave_height(x + 0.5, y + 0.5, tile_size)
            pixels[x, y] = (
                clamp_byte((nx * 0.5 + 0.5) * 255.0),
                clamp_byte((ny * 0.5 + 0.5) * 255.0),
                242,
                clamp_byte(h * 255.0),
            )
    return image


def build_water_specular_tile(tile_size):
    image = Image.new("RGBA", (tile_size, tile_size), (0, 0, 0, 0))
    pixels = image.load()
    for y in range(tile_size):
        for x in range(tile_size):
            h = water_wave_height(x + 0.5, y + 0.5, tile_size)
            sparkle = max(0.0, h - 0.68) / 0.32
            smoothness = 178.0 + sparkle * 54.0
            f0 = 14.0 + sparkle * 10.0
            porosity = 0.0
            pixels[x, y] = (clamp_byte(smoothness), clamp_byte(f0), clamp_byte(porosity), 0)
    return image


def resize_tile(image, tile_size):
    if image.height > image.width and image.height % image.width == 0:
        image = image.crop((0, 0, image.width, image.width))
    if image.size == (tile_size, tile_size):
        return image.copy()
    return image.resize((tile_size, tile_size), Image.Resampling.LANCZOS)


def frame_tile(image, frame_index):
    frame_size = min(image.width, image.height)
    columns = max(1, image.width // frame_size)
    x = (frame_index % columns) * frame_size
    y = (frame_index // columns) * frame_size
    return image.crop((x, y, x + frame_size, y + frame_size))


def read_animation(zip_file, texture_path, image):
    metadata = zip_read_optional(zip_file, texture_path + ".mcmeta")
    frame_size = min(image.width, image.height)
    if metadata is None or frame_size <= 0 or image.width % frame_size != 0 or image.height % frame_size != 0:
        return None

    total_source_frames = (image.width // frame_size) * (image.height // frame_size)
    if total_source_frames <= 1:
        return None

    data = json.loads(metadata.decode("utf-8"))
    animation = data.get("animation", {})
    default_ticks = int(animation.get("frametime", 1))
    entries = animation.get("frames")
    frames = []
    frame_ticks = []
    if entries is None:
        frames = list(range(total_source_frames))
        frame_ticks = [default_ticks] * total_source_frames
    else:
        for entry in entries:
            if isinstance(entry, int):
                frames.append(entry)
                frame_ticks.append(default_ticks)
            else:
                frames.append(int(entry["index"]))
                frame_ticks.append(int(entry.get("time", default_ticks)))

    valid_frames = []
    valid_ticks = []
    for frame, ticks in zip(frames, frame_ticks):
        if 0 <= frame < total_source_frames and ticks > 0:
            valid_frames.append(frame)
            valid_ticks.append(ticks)
    if len(valid_frames) <= 1:
        return None
    return valid_frames, valid_ticks


def tint_tile(image, tint):
    red, green, blue, alpha = image.split()
    red = ImageEnhance.Brightness(red).enhance(tint[0])
    green = ImageEnhance.Brightness(green).enhance(tint[1])
    blue = ImageEnhance.Brightness(blue).enhance(tint[2])
    return Image.merge("RGBA", (red, green, blue, alpha))


def build_grass_side_tile(zip_file, tile_size):
    base = find_texture(zip_file, ["grass_block_side.png"])
    overlay = find_texture(zip_file, ["grass_block_side_overlay.png"])
    if base is None or overlay is None:
        return None
    tile = resize_tile(base, tile_size)
    tinted_overlay = tint_tile(resize_tile(overlay, tile_size), GRASS_BIOME_TINT)
    return Image.alpha_composite(tile, tinted_overlay)

def build_grass_side_pbr_tile(zip_file, tile_size, suffix):
    base = find_texture(zip_file, ["grass_block_side.png"], suffix)
    overlay = find_texture(zip_file, ["grass_block_side_overlay.png"], suffix)
    overlay_mask = find_texture(zip_file, ["grass_block_side_overlay.png"])
    if base is None or overlay is None or overlay_mask is None:
        return None
    tile = resize_tile(base, tile_size)
    overlay_tile = resize_tile(overlay, tile_size)
    mask = resize_tile(overlay_mask, tile_size).getchannel("A")
    if suffix == "_n":
        return blend_normal_tiles(tile, overlay_tile, mask)
    tile.paste(overlay_tile, (0, 0), mask)
    return tile


def blend_normal_tiles(base, overlay, mask):
    out = Image.new("RGBA", base.size)
    base_pixels = base.load()
    overlay_pixels = overlay.load()
    mask_pixels = mask.load()
    out_pixels = out.load()
    for y in range(base.height):
        for x in range(base.width):
            weight = mask_pixels[x, y] / 255.0
            base_pixel = base_pixels[x, y]
            overlay_pixel = overlay_pixels[x, y]
            bx = base_pixel[0] / 255.0 * 2.0 - 1.0
            by = base_pixel[1] / 255.0 * 2.0 - 1.0
            ox = overlay_pixel[0] / 255.0 * 2.0 - 1.0
            oy = overlay_pixel[1] / 255.0 * 2.0 - 1.0
            nx = bx * (1.0 - weight) + ox * weight
            ny = by * (1.0 - weight) + oy * weight
            normal_length = (nx * nx + ny * ny) ** 0.5
            if normal_length > 0.985:
                nx *= 0.985 / normal_length
                ny *= 0.985 / normal_length
            ao = round(base_pixel[2] * (1.0 - weight) + overlay_pixel[2] * weight)
            height = round(base_pixel[3] * (1.0 - weight) + overlay_pixel[3] * weight)
            out_pixels[x, y] = (
                round((nx * 0.5 + 0.5) * 255.0),
                round((ny * 0.5 + 0.5) * 255.0),
                ao,
                height,
            )
    return out


def build_tile(zip_file, index, tile_size, warnings):
    if index == 0:
        return make_solid_tile((0, 0, 0, 0), tile_size)
    if index not in ATLAS_SOURCES and index not in SOLID_FALLBACKS and index not in SPRITE_FALLBACKS:
        return make_solid_tile((0, 0, 0, 0), tile_size)
    if zip_file is None:
        if index == 16 or index == 26:
            return build_water_tile(tile_size)
        if index in SPRITE_FALLBACKS:
            return make_sprite_tile(SPRITE_FALLBACKS[index], tile_size)
        return make_solid_tile(SOLID_FALLBACKS.get(index, (255, 0, 255, 255)), tile_size)
    if index == 9:
        return make_solid_tile(SOLID_FALLBACKS[index], tile_size)
    if index == 16 or index == 26:
        water = find_texture(zip_file, ATLAS_SOURCES[index][1])
        return resize_tile(water, tile_size) if water is not None else build_water_tile(tile_size)
    if index == 2:
        grass_side = build_grass_side_tile(zip_file, tile_size)
        if grass_side is not None:
            return grass_side

    source = ATLAS_SOURCES.get(index)
    image = find_texture(zip_file, source[1]) if source else None
    if image is None:
        warnings.append(f"atlas layer {index}: missing {source[0] if source else 'source'}, using fallback")
        if index in SPRITE_FALLBACKS:
            return make_sprite_tile(SPRITE_FALLBACKS[index], tile_size)
        return make_solid_tile(SOLID_FALLBACKS.get(index, (255, 0, 255, 255)), tile_size)

    tile = resize_tile(image, tile_size)
    if index == 1:
        tile = tint_tile(tile, GRASS_BIOME_TINT)
    elif index == 10:
        tile = tint_tile(tile, OAK_LEAVES_TINT)
    elif index == 15:
        tile = tint_tile(tile, BUSH_TINT)
    if index in TORCH_TINTS:
        tile = tint_tile(tile, TORCH_TINTS[index])
    return tile


def build_normal_tile(zip_file, index, tile_size, warnings):
    if index == 0 or (index not in ATLAS_SOURCES and index not in SOLID_FALLBACKS and index not in SPRITE_FALLBACKS):
        return make_solid_tile((128, 128, 255, 255), tile_size)
    if zip_file is None:
        if index == 16 or index == 26:
            return build_water_normal_tile(tile_size)
        return make_solid_tile((128, 128, 255, 255), tile_size)
    if index == 16 or index == 26:
        return build_water_normal_tile(tile_size)
    source = ATLAS_SOURCES.get(index)
    if index == 2:
        grass_side = build_grass_side_pbr_tile(zip_file, tile_size, "_n")
        if grass_side is not None:
            return grass_side
    image = find_texture(zip_file, source[1], "_n") if source else None
    if image is None:
        if source:
            warnings.append(f"normal layer {index}: missing {source[0]}_n, using flat fallback")
        return make_solid_tile((128, 128, 255, 255), tile_size)
    return resize_tile(image, tile_size)


def build_specular_tile(zip_file, index, tile_size, warnings):
    if index == 0 or (index not in ATLAS_SOURCES and index not in SOLID_FALLBACKS and index not in SPRITE_FALLBACKS):
        return make_solid_tile((32, 0, 0, 0), tile_size)
    if zip_file is None:
        if index == 16 or index == 26:
            return build_water_specular_tile(tile_size)
        return make_solid_tile((32, 0, 0, 0), tile_size)
    if index == 16 or index == 26:
        return build_water_specular_tile(tile_size)
    source = ATLAS_SOURCES.get(index)
    if index == 2:
        grass_side = build_grass_side_pbr_tile(zip_file, tile_size, "_s")
        if grass_side is not None:
            return grass_side
    image = find_texture(zip_file, source[1], "_s") if source else None
    if image is None:
        if source:
            warnings.append(f"specular layer {index}: missing {source[0]}_s, using neutral fallback")
        return make_solid_tile((32, 0, 0, 0), tile_size)
    return resize_tile(image, tile_size)


def save_atlas(path, tile_size, layer_count, builder, zip_file, warnings):
    atlas = Image.new("RGBA", (tile_size * layer_count, tile_size), (0, 0, 0, 0))
    for index in range(layer_count):
        atlas.paste(builder(zip_file, index, tile_size, warnings), (index * tile_size, 0))
    if atlas.mode != "RGBA" or atlas.size != (tile_size * layer_count, tile_size):
        raise RuntimeError(f"invalid atlas output {path}: mode={atlas.mode} size={atlas.size}")
    path.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(path)


def collect_animations(zip_file, tile_size, layer_count, warnings):
    if zip_file is None:
        return [], []

    animations = []
    frames = []
    for index in range(layer_count):
        source = ATLAS_SOURCES.get(index)
        if source is None:
            continue
        image, texture_path = find_texture_entry(zip_file, source[1])
        if image is None:
            continue
        animation = read_animation(zip_file, texture_path, image)
        if animation is None:
            continue
        source_frames, frame_ticks = animation
        first_frame = len(frames)
        for source_frame in source_frames:
            frames.append(resize_tile(frame_tile(image, source_frame), tile_size))
        animations.append({
            "layer": index,
            "name": source[0],
            "first_frame": first_frame,
            "frame_count": len(source_frames),
            "frame_ticks": frame_ticks,
        })
        warnings.append(f"animation layer {index}: {source[0]} frames={len(source_frames)}")
    return animations, frames


def save_animation_atlas(path, frames, tile_size):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not frames:
        Image.new("RGBA", (tile_size, tile_size), (0, 0, 0, 0)).save(path)
        return
    image = Image.new("RGBA", (tile_size * len(frames), tile_size), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        image.paste(frame, (index * tile_size, 0))
    image.save(path)


def write_animation_manifest(path, animations, frames, tile_size):
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "Octaryn generated atlas animations",
        f"tile_size={tile_size}",
        f"frames={len(frames)}",
        f"animations={len(animations)}",
    ]
    for animation in animations:
        ticks = ",".join(str(value) for value in animation["frame_ticks"])
        lines.append(
            "animation="
            f"{animation['layer']}|{animation['name']}|{animation['first_frame']}|"
            f"{animation['frame_count']}|{ticks}"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def license_metadata(args, warnings):
    if args.fallback_only:
        return {
            "texture_pack_name": args.pack_name or FALLBACK_PACK_NAME,
            "texture_pack_source_url": args.pack_source_url or FALLBACK_PACK_SOURCE_URL,
            "texture_pack_license_name": args.pack_license_name or FALLBACK_PACK_LICENSE_NAME,
            "texture_pack_license_url": args.pack_license_url or FALLBACK_PACK_LICENSE_URL,
            "texture_pack_license_file": args.pack_license_file or "",
            "texture_pack_credit": args.pack_credit or FALLBACK_PACK_CREDIT,
            "texture_pack_credits_url": args.pack_credits_url or "",
            "texture_pack_license_notice": "Fallback atlas is generated from Octaryn-owned color rules.",
        }

    use_default_pack = not args.pack

    def value(cli_value, default_value):
        if cli_value:
            return cli_value
        return default_value if use_default_pack else "UNSPECIFIED"

    metadata = {
        "texture_pack_name": value(args.pack_name, DEFAULT_PACK_NAME),
        "texture_pack_source_url": value(args.pack_source_url, DEFAULT_PACK_SOURCE_URL),
        "texture_pack_license_name": value(args.pack_license_name, DEFAULT_PACK_LICENSE_NAME),
        "texture_pack_license_url": value(args.pack_license_url, DEFAULT_PACK_LICENSE_URL),
        "texture_pack_license_file": value(args.pack_license_file, DEFAULT_PACK_LICENSE_FILE),
        "texture_pack_credit": value(args.pack_credit, DEFAULT_PACK_CREDIT),
        "texture_pack_credits_url": value(args.pack_credits_url, DEFAULT_PACK_CREDITS_URL),
        "texture_pack_license_notice": TEXTURE_PACK_LICENSE_NOTICE,
    }
    required_keys = (
        "texture_pack_name",
        "texture_pack_source_url",
        "texture_pack_license_name",
        "texture_pack_license_url",
        "texture_pack_license_file",
        "texture_pack_credit",
    )
    if any(metadata[key] == "UNSPECIFIED" for key in required_keys):
        warnings.append(
            "texture pack license metadata is incomplete; do not distribute generated atlas assets"
        )
    return metadata


def write_manifest(
    path,
    pack_path,
    outputs,
    tile_size,
    layer_count,
    texture_license,
    warnings,
):
    manifest = path.with_suffix(".txt")
    lines = [
        "Octaryn generated texture atlas",
        f"pack={pack_path}",
        f"layers={layer_count}",
        f"tile_size={tile_size}",
    ]
    lines.extend(f"{key}={value}" for key, value in texture_license.items())
    lines.extend(f"output={output}" for output in outputs)
    lines.extend(f"warning={warning}" for warning in warnings)
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    args = parse_args()
    if args.tile_size <= 0:
        raise RuntimeError("--tile-size must be positive")
    if args.layer_count <= 0:
        raise RuntimeError("--layer-count must be positive")
    cache_dir = Path(args.cache_dir)
    output = Path(args.output)
    if args.fallback_only:
        pack_path = Path("fallback-only")
    else:
        pack_path = Path(args.pack) if args.pack else download_pack(args.url, cache_dir)
        verify_sha256(pack_path, args.sha256)

    outputs = [Path(args.output)]
    if args.normal_output:
        outputs.append(Path(args.normal_output))
    if args.specular_output:
        outputs.append(Path(args.specular_output))
    if args.animation_output:
        outputs.append(Path(args.animation_output))
    if args.animation_manifest:
        outputs.append(Path(args.animation_manifest))

    warnings = []
    texture_license = license_metadata(args, warnings)
    if args.fallback_only:
        save_atlas(Path(args.output), args.tile_size, args.layer_count, build_tile, None, warnings)
        if args.normal_output:
            save_atlas(Path(args.normal_output), args.tile_size, args.layer_count, build_normal_tile, None, warnings)
        if args.specular_output:
            save_atlas(Path(args.specular_output), args.tile_size, args.layer_count, build_specular_tile, None, warnings)
        animations, frames = collect_animations(None, args.tile_size, args.layer_count, warnings)
        if args.animation_output:
            save_animation_atlas(Path(args.animation_output), frames, args.tile_size)
        if args.animation_manifest:
            write_animation_manifest(Path(args.animation_manifest), animations, frames, args.tile_size)
    else:
        with zipfile.ZipFile(pack_path) as zip_file:
            save_atlas(Path(args.output), args.tile_size, args.layer_count, build_tile, zip_file, warnings)
            if args.normal_output:
                save_atlas(Path(args.normal_output), args.tile_size, args.layer_count, build_normal_tile, zip_file, warnings)
            if args.specular_output:
                save_atlas(Path(args.specular_output), args.tile_size, args.layer_count, build_specular_tile, zip_file, warnings)
            animations, frames = collect_animations(zip_file, args.tile_size, args.layer_count, warnings)
            if args.animation_output:
                save_animation_atlas(Path(args.animation_output), frames, args.tile_size)
            if args.animation_manifest:
                write_animation_manifest(Path(args.animation_manifest), animations, frames, args.tile_size)
    write_manifest(output, pack_path, outputs, args.tile_size, args.layer_count, texture_license, warnings)
    for warning in warnings:
        print(f"[atlas] {warning}", file=sys.stderr)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[atlas] failed: {exc}", file=sys.stderr)
        sys.exit(1)
