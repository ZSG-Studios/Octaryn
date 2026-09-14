from __future__ import annotations

import json


def load_blocks(catalog_path):
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    blocks = catalog.get("blocks")
    if not isinstance(blocks, list):
        raise ValueError(f"{catalog_path}: blocks must be a list")
    return blocks


def render_atlas_layers_source(catalog_path):
    blocks = load_blocks(catalog_path)
    lines = [
        "using Octaryn.Shared.World;",
        "",
        "namespace Octaryn.Basegame.Content.Blocks;",
        "",
        "public static partial class BlockCatalog",
        "{",
        "    private static readonly BlockAtlasFaceLayers[] AtlasFaceLayers =",
        "    [",
    ]
    for index, block in enumerate(blocks):
        atlas = required_atlas(catalog_path, index, block)
        line = (
            "        new BlockAtlasFaceLayers("
            f"{atlas['north']}, {atlas['south']}, {atlas['east']}, "
            f"{atlas['west']}, {atlas['up']}, {atlas['down']})")
        lines.append(f"{line}," if index < len(blocks) - 1 else line)
    lines.extend([
        "    ];",
        "",
        "    public static BlockAtlasFaceLayers AtlasLayers(BlockId block)",
        "    {",
        "        return block.Value < AtlasFaceLayers.Length",
        "            ? AtlasFaceLayers[block.Value]",
        "            : default;",
        "    }",
        "",
        "    public static ushort AtlasLayer(BlockId block, BlockFace face)",
        "    {",
        "        return AtlasLayers(block).LayerFor(face);",
        "    }",
        "}",
    ])
    return "\n".join(lines) + "\n"


def required_atlas(catalog_path, index, block):
    atlas = block.get("atlas")
    if not isinstance(atlas, dict):
        raise ValueError(f"{catalog_path}: block index {index} must define atlas layers")
    return atlas
