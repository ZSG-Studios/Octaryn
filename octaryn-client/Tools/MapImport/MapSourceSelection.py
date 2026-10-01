"""Avoid importing alternative versions of the same authored Bistro interior."""
from pathlib import Path


def select_sources(paths: list[Path], interior: str = 'wine') -> tuple[list[Path], list[Path]]:
    selected = list(dict.fromkeys(path.resolve() for path in paths))
    excluded = []
    for folder in {path.parent for path in selected}:
        variants = {path.name.lower(): path for path in selected if path.parent == folder}
        if 'bistrointerior.fbx' in variants and 'bistrointerior_wine.fbx' in variants:
            # The source README identifies Wine as a modified Interior, not an extension.
            name = 'bistrointerior.fbx' if interior == 'wine' else 'bistrointerior_wine.fbx'
            excluded.append(variants[name])
    return [path for path in selected if path not in excluded], excluded
