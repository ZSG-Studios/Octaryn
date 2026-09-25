using System;
using System.Collections.Generic;

namespace Octaryn.Shared.World;

// Content identities supplied once by a module; hosts additionally validate
// catalog membership. Defensive copies prevent later mutation.
public sealed class FluidRules
{
    public IReadOnlyList<BlockId> WaterLevels { get; }
    public IReadOnlyList<BlockId> LavaLevels { get; }
    public BlockId Stone { get; }
    public IReadOnlyList<BlockId> Replaceable { get; }
    public IReadOnlyList<BlockId> Solid { get; }

    public FluidRules(BlockId[] waterLevels, BlockId[] lavaLevels, BlockId stone,
        BlockId[] replaceable, BlockId[] solid)
    {
        ArgumentNullException.ThrowIfNull(waterLevels);
        ArgumentNullException.ThrowIfNull(lavaLevels);
        ArgumentNullException.ThrowIfNull(replaceable);
        ArgumentNullException.ThrowIfNull(solid);
        if (waterLevels.Length != 8 || lavaLevels.Length != 8)
            throw new ArgumentException("Fluids require eight level IDs, with source at level zero.");
        var water = (BlockId[])waterLevels.Clone();
        var lava = (BlockId[])lavaLevels.Clone();
        var replace = (BlockId[])replaceable.Clone();
        var solids = (BlockId[])solid.Clone();
        var levels = new HashSet<BlockId>();
        foreach (var table in new[] { water, lava })
            foreach (var block in table)
                if (block == BlockId.Air || !levels.Add(block))
                    throw new ArgumentException("Fluid level IDs must be nonzero and distinct across both fluids.");
        if (stone == BlockId.Air || levels.Contains(stone))
            throw new ArgumentException("Fluid contact stone must be a non-fluid block.");
        var replaceSet = ValidateMembership(replace, levels);
        var solidSet = ValidateMembership(solids, levels);
        if (!solidSet.Contains(stone) || replaceSet.Contains(stone))
            throw new ArgumentException("Contact stone must be solid and not fluid-replaceable.");
        WaterLevels = Array.AsReadOnly(water);
        LavaLevels = Array.AsReadOnly(lava);
        Stone = stone;
        Replaceable = Array.AsReadOnly(replace);
        Solid = Array.AsReadOnly(solids);
    }

    private static HashSet<BlockId> ValidateMembership(BlockId[] table, HashSet<BlockId> levels)
    {
        if (table.Length > ushort.MaxValue)
            throw new ArgumentException("Fluid membership list exceeds the block ID domain.");
        var found = new HashSet<BlockId>();
        foreach (var block in table)
            if (block == BlockId.Air || levels.Contains(block) || !found.Add(block))
                throw new ArgumentException("Fluid membership lists require distinct nonzero non-fluid IDs.");
        // Solid/replaceable overlap is allowed: original fluids replace leaves.
        return found;
    }
}
