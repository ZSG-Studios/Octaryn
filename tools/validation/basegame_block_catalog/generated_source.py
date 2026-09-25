from __future__ import annotations

import subprocess
import sys


def validate_generated_source(errors, catalog_path, source_path):
    if source_path.name != "BlockCatalog.cs":
        errors.append(f"{source_path}: generated block catalog source must be BlockCatalog.cs")
        return
    if not source_path.exists():
        errors.append(f"{source_path}: generated block catalog source is missing")
        return

    validate_artifact(errors, catalog_path, source_path, "catalog")
    validate_artifact(
        errors,
        catalog_path,
        source_path.with_name("BlockCatalog.AtlasLayers.cs"),
        "atlas-layers")


def validate_artifact(errors, catalog_path, artifact_path, artifact):
    if not artifact_path.exists():
        errors.append(f"{artifact_path}: generated {artifact} artifact is missing")
        return

    generator_path = catalog_path.parents[2] / "Tools" / "generate_block_catalog_source.py"
    result = subprocess.run(
        [
            sys.executable,
            str(generator_path),
            "--catalog",
            str(catalog_path),
            "--output",
            "-",
            "--artifact",
            artifact,
        ],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True)
    if result.returncode != 0:
        errors.append(
            f"{generator_path}: failed to render {artifact}: "
            f"{result.stderr.strip()}")
        return

    actual = artifact_path.read_text(encoding="utf-8")
    if actual != result.stdout:
        errors.append(f"{artifact_path}: generated {artifact} does not match {catalog_path}")
