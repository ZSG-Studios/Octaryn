using System.Collections.Generic;
using Octaryn.Basegame.Content.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Basegame.Content.Fluids;

public static class BasegameFluidRules
{
    public static FluidRules Create()
    {
        var water = new BlockId[8];
        var lava = new BlockId[8];
        for (var level = 0; level < 8; ++level)
        {
            water[level] = BlockCatalog.MakeWater(level);
            lava[level] = BlockCatalog.MakeLava(level);
        }
        var replaceable = new List<BlockId>();
        var solid = new List<BlockId>();
        for (ushort id = 1; id < BlockCatalog.KnownBlockCount; ++id)
        {
            var block = new BlockId(id);
            // Original world/edit/water.cpp: leaves or grass-dependent plants.
            if (block == BlockCatalog.Leaves || BlockCatalog.RequiresGrass(block)) replaceable.Add(block);
            if (BlockCatalog.IsSolid(block)) solid.Add(block);
        }
        return new FluidRules(water, lava, BlockCatalog.Stone, replaceable.ToArray(), solid.ToArray());
    }
}
