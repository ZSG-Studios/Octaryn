from PIL import Image

from .sources import (
    DEFAULT_PACK_CREDIT,
    DEFAULT_PACK_CREDITS_URL,
    DEFAULT_PACK_LICENSE_FILE,
    DEFAULT_PACK_LICENSE_NAME,
    DEFAULT_PACK_LICENSE_URL,
    DEFAULT_PACK_NAME,
    DEFAULT_PACK_SOURCE_URL,
    FALLBACK_PACK_CREDIT,
    FALLBACK_PACK_LICENSE_NAME,
    FALLBACK_PACK_LICENSE_URL,
    FALLBACK_PACK_NAME,
    FALLBACK_PACK_SOURCE_URL,
    TEXTURE_PACK_LICENSE_NOTICE,
)


def save_atlas(path, tile_size, layer_count, builder, zip_file, warnings):
    atlas = Image.new("RGBA", (tile_size * layer_count, tile_size), (0, 0, 0, 0))
    for index in range(layer_count):
        atlas.paste(builder(zip_file, index, tile_size, warnings), (index * tile_size, 0))
    if atlas.mode != "RGBA" or atlas.size != (tile_size * layer_count, tile_size):
        raise RuntimeError(f"invalid atlas output {path}: mode={atlas.mode} size={atlas.size}")
    path.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(path)


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
