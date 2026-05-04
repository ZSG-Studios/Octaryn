from __future__ import annotations

import argparse
import pathlib
import sys

from .atlas_assets import validate_animation_assets, validate_atlas_assets
from .catalog import validate_catalog
from .generated_source import validate_generated_source


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", required=True)
    parser.add_argument("--generated-source")
    parser.add_argument("--atlas-color")
    parser.add_argument("--atlas-normal")
    parser.add_argument("--atlas-specular")
    parser.add_argument("--animation-atlas")
    parser.add_argument("--animation-manifest")
    args = parser.parse_args()

    catalog_path = pathlib.Path(args.catalog)
    errors = validate_catalog(catalog_path)
    if args.generated_source:
        validate_generated_source(errors, catalog_path, pathlib.Path(args.generated_source))
    validate_optional_atlases(errors, args)
    validate_optional_animation(errors, args)
    if errors:
        for error in errors:
            print(f"basegame block catalog policy: {error}", file=sys.stderr)
        return 1
    return 0


def validate_optional_atlases(errors, args):
    if not (args.atlas_color or args.atlas_normal or args.atlas_specular):
        return
    required_atlases = (
        ("color", args.atlas_color),
        ("normal", args.atlas_normal),
        ("specular", args.atlas_specular),
    )
    for label, value in required_atlases:
        if not value:
            errors.append(f"--atlas-{label} is required when validating atlas assets")
    if all(value for _label, value in required_atlases):
        validate_atlas_assets(
            errors,
            [(label, pathlib.Path(value)) for label, value in required_atlases])


def validate_optional_animation(errors, args):
    if not (args.animation_atlas or args.animation_manifest):
        return
    if not args.animation_atlas:
        errors.append("--animation-atlas is required when validating animation assets")
    if not args.animation_manifest:
        errors.append("--animation-manifest is required when validating animation assets")
    if args.animation_atlas and args.animation_manifest:
        validate_animation_assets(
            errors,
            pathlib.Path(args.animation_atlas),
            pathlib.Path(args.animation_manifest))
