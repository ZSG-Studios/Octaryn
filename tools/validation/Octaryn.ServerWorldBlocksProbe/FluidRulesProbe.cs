using System;
using System.Collections.Generic;
using Octaryn.Basegame.Content.Blocks;
using Octaryn.Basegame.Module;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.World;

internal static partial class ServerWorldBlocksProbe
{
    private static void ValidateFluidRules()
    {
        var checks = 0;
        void Check(bool result, string reason)
        {
            ++checks;
            if (!result) throw new InvalidOperationException(reason);
        }
        void Reject(Action construct)
        {
            var rejected = false;
            try { construct(); }
            catch (ArgumentException) { rejected = true; }
            Check(rejected, "Malformed fluid rules were accepted.");
        }
        IFluidRulesProvider provider = new ModuleRegistration();
        var rules = provider.FluidRules;
        Check(rules.WaterLevels.Count == 8 && rules.LavaLevels.Count == 8, "Basegame fluid level count.");
        for (var i = 0; i < 8; ++i)
        {
            Check(rules.WaterLevels[i] == new BlockId((ushort)(14 + i)), "Basegame water IDs.");
            Check(rules.LavaLevels[i] == new BlockId((ushort)(31 + i)), "Basegame lava IDs.");
            Check(BlockCatalog.FluidLevel(rules.WaterLevels[i]) == i &&
                BlockCatalog.FluidLevel(rules.LavaLevels[i]) == i, "Provider/catalog fluid level parity.");
        }
        Check(rules.Stone == BlockCatalog.Stone, "Basegame contact product.");
        var replaced = new HashSet<BlockId>(rules.Replaceable);
        var solids = new HashSet<BlockId>(rules.Solid);
        for (ushort i = 1; i < BlockCatalog.KnownBlockCount; ++i)
        {
            var block = new BlockId(i);
            Check(replaced.Contains(block) == (block == BlockCatalog.Leaves || BlockCatalog.RequiresGrass(block)),
                "Original leaves/grass replacement membership.");
            Check(solids.Contains(block) == BlockCatalog.IsSolid(block), "Source support solid membership.");
        }
        foreach (var list in new[] { rules.WaterLevels, rules.LavaLevels, rules.Replaceable, rules.Solid })
            foreach (var block in list) Check(BlockCatalog.IsKnown(block), "Provider emitted an unknown block ID.");
        Check(replaced.Contains(BlockCatalog.Leaves) && solids.Contains(BlockCatalog.Leaves),
            "Valid solid/replaceable overlap must survive.");

        BlockId[] Water() => [new(100), new(101), new(102), new(103), new(104), new(105), new(106), new(107)];
        BlockId[] Lava() => [new(200), new(201), new(202), new(203), new(204), new(205), new(206), new(207)];
        var water = Water(); var lava = Lava();
        BlockId[] replace = [new(7)]; BlockId[] solid = [new(3), new(7)];
        var owned = new FluidRules(water, lava, new(3), replace, solid);
        water[0] = lava[0] = replace[0] = solid[0] = new(999);
        Check(owned.WaterLevels[0] == new BlockId(100) && owned.LavaLevels[0] == new BlockId(200) &&
            owned.Replaceable[0] == new BlockId(7) && owned.Solid[0] == new BlockId(3), "Rules must own input copies.");
        foreach (var list in new[] { owned.WaterLevels, owned.LavaLevels, owned.Replaceable, owned.Solid })
        {
            Check(list is not BlockId[], "Rules must not expose their mutable arrays.");
            var mutationRejected = false;
            try { ((IList<BlockId>)list)[0] = new(999); }
            catch (NotSupportedException) { mutationRejected = true; }
            Check(mutationRejected, "Read-only rules list accepted mutation.");
        }
        Reject(() => new FluidRules(null!, Lava(), new(3), [], [new(3)]));
        Reject(() => new FluidRules(Water(), null!, new(3), [], [new(3)]));
        Reject(() => new FluidRules(Water(), Lava(), new(3), null!, [new(3)]));
        Reject(() => new FluidRules(Water(), Lava(), new(3), [], null!));
        Reject(() => new FluidRules([], Lava(), new(3), [], [new(3)]));
        Reject(() => new FluidRules(Water(), [], new(3), [], [new(3)]));
        var invalid = Water(); invalid[0] = BlockId.Air;
        Reject(() => new FluidRules(invalid, Lava(), new(3), [], [new(3)]));
        invalid = Water(); invalid[1] = invalid[0];
        Reject(() => new FluidRules(invalid, Lava(), new(3), [], [new(3)]));
        var overlap = Lava(); overlap[7] = Water()[0];
        Reject(() => new FluidRules(Water(), overlap, new(3), [], [new(3)]));
        Reject(() => new FluidRules(Water(), Lava(), BlockId.Air, [], [new(3)]));
        Reject(() => new FluidRules(Water(), Lava(), new(100), [], [new(3)]));
        Reject(() => new FluidRules(Water(), Lava(), new(3), [], []));
        foreach (var bad in new BlockId[][] { [BlockId.Air], [new(7), new(7)], [new(100)], [new(3)] })
            Reject(() => new FluidRules(Water(), Lava(), new(3), bad, [new(3)]));
        foreach (var bad in new BlockId[][] { [BlockId.Air], [new(3), new(3)], [new(3), new(200)] })
            Reject(() => new FluidRules(Water(), Lava(), new(3), [], bad));
        Console.WriteLine($"fluid_rules_provider=passed checks={checks} provider_fixture=1");
    }
}
