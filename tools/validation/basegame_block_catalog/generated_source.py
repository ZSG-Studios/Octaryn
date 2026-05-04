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

    generator_path = catalog_path.parents[2] / "Tools" / "generate_block_catalog_source.py"
    result = subprocess.run(
        [sys.executable, str(generator_path), "--catalog", str(catalog_path), "--output", "-"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True)
    if result.returncode != 0:
        errors.append(f"{generator_path}: failed to render generated source: {result.stderr.strip()}")
        return

    actual = source_path.read_text(encoding="utf-8")
    if actual != result.stdout:
        errors.append(f"{source_path}: generated block catalog source does not match {catalog_path}")
