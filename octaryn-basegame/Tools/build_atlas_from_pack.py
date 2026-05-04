#!/usr/bin/env python3
import argparse
import sys
import zipfile
from pathlib import Path

from atlas_builder.animations import collect_animations, save_animation_atlas, write_animation_manifest
from atlas_builder.outputs import license_metadata, save_atlas, write_manifest
from atlas_builder.pack_io import download_pack, verify_sha256
from atlas_builder.sources import DEFAULT_PACK_URL
from atlas_builder.tiles import build_normal_tile, build_specular_tile, build_tile


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


def output_paths(args):
    outputs = [Path(args.output)]
    if args.normal_output:
        outputs.append(Path(args.normal_output))
    if args.specular_output:
        outputs.append(Path(args.specular_output))
    if args.animation_output:
        outputs.append(Path(args.animation_output))
    if args.animation_manifest:
        outputs.append(Path(args.animation_manifest))
    return outputs


def write_outputs(args, pack_path, zip_file, warnings):
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
    texture_license = license_metadata(args, warnings)
    write_manifest(
        Path(args.output),
        pack_path,
        output_paths(args),
        args.tile_size,
        args.layer_count,
        texture_license,
        warnings,
    )


def main():
    args = parse_args()
    if args.tile_size <= 0:
        raise RuntimeError("--tile-size must be positive")
    if args.layer_count <= 0:
        raise RuntimeError("--layer-count must be positive")

    if args.fallback_only:
        pack_path = Path("fallback-only")
        write_outputs(args, pack_path, None, [])
        return

    cache_dir = Path(args.cache_dir)
    pack_path = Path(args.pack) if args.pack else download_pack(args.url, cache_dir)
    verify_sha256(pack_path, args.sha256)
    warnings = []
    with zipfile.ZipFile(pack_path) as zip_file:
        write_outputs(args, pack_path, zip_file, warnings)
    for warning in warnings:
        print(f"[atlas] {warning}", file=sys.stderr)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[atlas] failed: {exc}", file=sys.stderr)
        sys.exit(1)
