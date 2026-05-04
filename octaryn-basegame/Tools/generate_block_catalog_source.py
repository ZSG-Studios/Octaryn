#!/usr/bin/env python3
import argparse
import json
import pathlib
import sys


def render(catalog_path):
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    blocks = catalog.get("blocks")
    if not isinstance(blocks, list):
        raise ValueError(f"{catalog_path}: blocks must be a list")

    block_ids = {}
    grass_supported_ids = []
    placeable_ids = []
    solid_base_supported_ids = []
    solid_ids = []
    targetable_ids = []
    fluid_kinds = []
    fluid_levels = []
    fluid_source_ids = []
    skylight_opacities = []
    for index, block in enumerate(blocks):
        if not isinstance(block, dict):
            raise ValueError(f"{catalog_path}: block index {index} must be an object")
        block_id = block.get("id")
        if not isinstance(block_id, str):
            raise ValueError(f"{catalog_path}: block index {index} has invalid id {block_id!r}")
        block_ids[block_id] = index
        if block.get("requiresGrass") is True:
            grass_supported_ids.append(index)
        if block.get("placeable") is True:
            placeable_ids.append(index)
        if block.get("requiresSolidBase") is True:
            solid_base_supported_ids.append(index)
        if block.get("solid") is True:
            solid_ids.append(index)
        if block.get("targetable") is True:
            targetable_ids.append(index)
        fluid_kind = block.get("fluidKind")
        fluid_level = block.get("fluidLevel")
        if fluid_kind not in {"none", "water", "lava"}:
            raise ValueError(f"{catalog_path}: block index {index} has invalid fluidKind {fluid_kind!r}")
        if not isinstance(fluid_level, int):
            raise ValueError(f"{catalog_path}: block index {index} has invalid fluidLevel {fluid_level!r}")
        fluid_kinds.append(fluid_kind_value(fluid_kind))
        fluid_levels.append(fluid_level)
        if block.get("fluidSource") is True:
            fluid_source_ids.append(index)
        skylight_opacity = block.get("skylightOpacity")
        if not isinstance(skylight_opacity, int) or skylight_opacity < 0 or skylight_opacity > 15:
            raise ValueError(f"{catalog_path}: block index {index} has invalid skylightOpacity {skylight_opacity!r}")
        skylight_opacities.append(skylight_opacity)

    lines = [
        "using Octaryn.Basegame.Content.Fluids;",
        "using Octaryn.Shared.World;",
        "",
        "namespace Octaryn.Basegame.Content.Blocks;",
        "",
        "public static class BlockCatalog",
        "{",
        f"    public const ushort KnownBlockCount = {len(blocks)};",
        "",
        "    private static readonly ushort[] GrassSupportedBlockIds =",
        "    [",
    ]
    lines.extend(render_span_values(grass_supported_ids))
    lines.extend([
        "",
        "    private static readonly ushort[] PlaceableBlockIds =",
        "    [",
    ])
    lines.extend(render_span_values(placeable_ids))
    lines.extend([
        "",
        "    private static readonly ushort[] SolidBaseSupportedBlockIds =",
        "    [",
    ])
    lines.extend(render_span_values(solid_base_supported_ids))
    lines.extend([
        "",
        "    private static readonly ushort[] SolidBlockIds =",
        "    [",
    ])
    lines.extend(render_span_values(solid_ids))
    lines.extend([
        "",
        "    private static readonly ushort[] TargetableBlockIds =",
        "    [",
    ])
    lines.extend(render_span_values(targetable_ids))
    lines.extend([
        "",
        "    private static readonly ushort[] FluidKindValues =",
        "    [",
    ])
    lines.extend(render_span_values(fluid_kinds))
    lines.extend([
        "",
        "    private static readonly int[] FluidLevels =",
        "    [",
    ])
    lines.extend(render_int_values(fluid_levels))
    lines.extend([
        "",
        "    private static readonly ushort[] FluidSourceBlockIds =",
        "    [",
    ])
    lines.extend(render_span_values(fluid_source_ids))
    lines.extend([
        "",
        "    private static readonly int[] SkylightOpacities =",
        "    [",
    ])
    lines.extend(render_int_values(skylight_opacities))
    lines.extend(render_members(block_ids))
    return "\n".join(lines)


def render_span_values(values):
    lines = []
    for value in values[:-1]:
        lines.append(f"        {value},")
    if values:
        lines.append(f"        {values[-1]}")
    lines.append("    ];")
    return lines


def render_int_values(values):
    lines = []
    for value in values[:-1]:
        lines.append(f"        {value},")
    if values:
        lines.append(f"        {values[-1]}")
    lines.append("    ];")
    return lines


def render_members(block_ids):
    block_member_ids = [
        ("Dirt", "octaryn.basegame.block.dirt"),
        ("Grass", "octaryn.basegame.block.grass"),
        ("Sand", "octaryn.basegame.block.sand"),
        ("Snow", "octaryn.basegame.block.snow"),
        ("Stone", "octaryn.basegame.block.stone"),
        ("Log", "octaryn.basegame.block.log"),
        ("Leaves", "octaryn.basegame.block.leaves"),
        ("Cloud", "octaryn.basegame.block.cloud"),
        ("Bush", "octaryn.basegame.block.bush"),
        ("Bluebell", "octaryn.basegame.block.bluebell"),
        ("Gardenia", "octaryn.basegame.block.gardenia"),
        ("Rose", "octaryn.basegame.block.rose"),
        ("Lavender", "octaryn.basegame.block.lavender"),
        ("RedTorch", "octaryn.basegame.block.red_torch"),
        ("GreenTorch", "octaryn.basegame.block.green_torch"),
        ("BlueTorch", "octaryn.basegame.block.blue_torch"),
        ("YellowTorch", "octaryn.basegame.block.yellow_torch"),
        ("CyanTorch", "octaryn.basegame.block.cyan_torch"),
        ("MagentaTorch", "octaryn.basegame.block.magenta_torch"),
        ("WhiteTorch", "octaryn.basegame.block.white_torch"),
        ("Planks", "octaryn.basegame.block.planks"),
        ("Glass", "octaryn.basegame.block.glass"),
    ]
    fluid_member_ids = [
        ("WaterSource", "octaryn.basegame.block.water"),
        ("WaterLevelOne", "octaryn.basegame.block.water_1"),
        ("WaterLevelSeven", "octaryn.basegame.block.water_7"),
        ("LavaSource", "octaryn.basegame.block.lava"),
        ("LavaLevelOne", "octaryn.basegame.block.lava_1"),
        ("LavaLevelSeven", "octaryn.basegame.block.lava_7"),
    ]
    named_block_ids = block_member_ids + fluid_member_ids
    named_ids = {
        name: required_block_id(block_ids, block_id)
        for name, block_id in named_block_ids
    }
    member_lines = []
    for name, _ in block_member_ids:
        member_lines.extend(["", f"    public static BlockId {name} => new({named_ids[name]});"])
    member_lines.extend([
        "",
        f"    public static BlockId DefaultSelectedBlock => new({named_ids['YellowTorch']});",
    ])
    for name, _ in fluid_member_ids:
        member_lines.extend(["", f"    public static BlockId {name} => new({named_ids[name]});"])

    return member_lines + [
        "",
        "    public static bool IsKnown(BlockId block)",
        "    {",
        "        return block.Value < KnownBlockCount;",
        "    }",
        "",
        "    public static bool IsPlaceable(BlockId block)",
        "    {",
        "        return IndexOf(PlaceableBlockIds, block.Value) >= 0;",
        "    }",
        "",
        "    public static bool IsSolid(BlockId block)",
        "    {",
        "        return IndexOf(SolidBlockIds, block.Value) >= 0;",
        "    }",
        "",
        "    public static bool IsTargetable(BlockId block)",
        "    {",
        "        return IndexOf(TargetableBlockIds, block.Value) >= 0;",
        "    }",
        "",
        "    public static bool IsFluid(BlockId block)",
        "    {",
        "        return GetFluidKind(block) != FluidKind.None;",
        "    }",
        "",
        "    public static bool IsFluidSource(BlockId block)",
        "    {",
        "        return IndexOf(FluidSourceBlockIds, block.Value) >= 0;",
        "    }",
        "",
        "    public static bool IsWater(BlockId block)",
        "    {",
        "        return GetFluidKind(block) == FluidKind.Water;",
        "    }",
        "",
        "    public static bool IsLava(BlockId block)",
        "    {",
        "        return GetFluidKind(block) == FluidKind.Lava;",
        "    }",
        "",
        "    public static FluidKind GetFluidKind(BlockId block)",
        "    {",
        "        return block.Value < FluidKindValues.Length",
        "            ? (FluidKind)FluidKindValues[block.Value]",
        "            : FluidKind.None;",
        "    }",
        "",
        "    public static int FluidLevel(BlockId block)",
        "    {",
        "        return block.Value < FluidLevels.Length ? FluidLevels[block.Value] : -1;",
        "    }",
        "",
        "    public static int SkylightOpacity(BlockId block)",
        "    {",
        "        return block.Value < SkylightOpacities.Length ? SkylightOpacities[block.Value] : 0;",
        "    }",
        "",
        "    public static BlockId MakeWater(int level)",
        "    {",
        f"        return new BlockId(ClampedFluidBlockId({named_ids['WaterSource']}, {named_ids['WaterLevelSeven']}, level));",
        "    }",
        "",
        "    public static BlockId MakeLava(int level)",
        "    {",
        f"        return new BlockId(ClampedFluidBlockId({named_ids['LavaSource']}, {named_ids['LavaLevelSeven']}, level));",
        "    }",
        "",
        "    public static BlockId MakeFluid(FluidKind kind, int level)",
        "    {",
        "        return kind switch",
        "        {",
        "            FluidKind.Water => MakeWater(level),",
        "            FluidKind.Lava => MakeLava(level),",
        "            _ => BlockId.Air",
        "        };",
        "    }",
        "",
        "    public static bool RequiresGrass(BlockId block)",
        "    {",
        "        return IndexOf(GrassSupportedBlockIds, block.Value) >= 0;",
        "    }",
        "",
        "    public static bool RequiresSolidBase(BlockId block)",
        "    {",
        "        return IndexOf(SolidBaseSupportedBlockIds, block.Value) >= 0;",
        "    }",
        "",
        "    private static ushort ClampedFluidBlockId(ushort sourceId, ushort maxLevelId, int level)",
        "    {",
        "        if (level <= 0)",
        "        {",
        "            return sourceId;",
        "        }",
        "",
        "        if (level >= 7)",
        "        {",
        "            return maxLevelId;",
        "        }",
        "",
        "        return (ushort)(sourceId + level);",
        "    }",
        "",
        "    private static int IndexOf(ushort[] values, ushort value)",
        "    {",
        "        for (var index = 0; index < values.Length; index++)",
        "        {",
        "            if (values[index] == value)",
        "            {",
        "                return index;",
        "            }",
        "        }",
        "",
        "        return -1;",
        "    }",
        "}",
        "",
    ]


def required_block_id(block_ids, block_id):
    block_value = block_ids.get(block_id)
    if block_value is None:
        raise ValueError(f"missing required block {block_id}")
    return block_value


def fluid_kind_value(fluid_kind):
    if fluid_kind == "water":
        return 1
    if fluid_kind == "lava":
        return 2
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    source = render(pathlib.Path(args.catalog))
    if args.output == "-":
        sys.stdout.write(source)
        return 0

    output_path = pathlib.Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(source, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
