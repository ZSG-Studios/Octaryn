import math

from PIL import Image, ImageEnhance

from .pack_io import find_texture
from .sources import (
    ATLAS_SOURCES,
    BUSH_TINT,
    GRASS_BIOME_TINT,
    OAK_LEAVES_TINT,
    SOLID_FALLBACKS,
    SPRITE_FALLBACKS,
    TORCH_TINTS,
)


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
