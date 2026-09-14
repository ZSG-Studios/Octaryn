using Octaryn.Basegame.Content.Worldgen;
using Octaryn.Basegame.Content.Blocks;
using Octaryn.Basegame.Module;
using Octaryn.Server;
using Octaryn.Server.Modules;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.World.Generation;
using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

return ServerWorldGenerationProbe.Run(args);

internal static class ServerWorldGenerationProbe
{
    public static int Run(string[] args)
    {
        ValidateBasegameRules();
        if (args.Contains("--basegame-only", StringComparer.Ordinal))
        {
            ValidateManifestCapabilities();
            Console.WriteLine("Basegame terrain material, sample contract and capability checks passed.");
            return 0;
        }
        ValidateServerGeneration();
        ValidateActivatorKeepsMissingWorldInMemory();
        ValidateActivatorCleansGeneratedOverrides();
        ValidateActivatorKeepsPersistedWorld();
        ValidateManifestCapabilities();
        return 0;
    }

    private static void ValidateBasegameRules()
    {
        var rules = new WorldGenerationRules();
        Require(rules.WaterHeight == 30, "water height matches compiled worldgen");
        Require(rules.WaterBlock == BlockCatalog.WaterSource, "water fill uses stable basegame water block");

        var sand = rules.PlanTerrainColumn(Sample(0, 0, 18, 0, 0));
        Require(sand.TerrainHeight == 18 && sand.DecorationY == 30, "sampled height is preserved below water");
        Require(sand.SurfaceBlock == BlockCatalog.Sand, "low terrain uses sand surface");
        Require(sand.FillBlock == BlockCatalog.Sand, "low terrain uses sand fill");

        var grass = rules.PlanTerrainColumn(Sample(1, 0, 45, 0, 0));
        Require(grass.SurfaceBlock == BlockCatalog.Grass, "mid lowland terrain uses grass surface");
        Require(grass.FillBlock == BlockCatalog.Dirt, "mid lowland terrain uses dirt fill");
        Require(grass.HasGrassSurface, "grass terrain accepts flora");

        var stone = rules.PlanTerrainColumn(Sample(2, 0, 106, 0, 0));
        Require(stone.SurfaceBlock == BlockCatalog.Stone, "high terrain uses stone surface");
        Require(stone.FillBlock == BlockCatalog.Stone, "high terrain uses stone fill");

        var snow = rules.PlanTerrainColumn(Sample(3, 0, 151, 1, 0));
        Require(snow.SurfaceBlock == BlockCatalog.Snow, "peak terrain uses snow surface");
        Require(snow.FillBlock == BlockCatalog.Stone, "peak terrain uses stone fill");

        ValidateMaterialBoundaries(rules);

    }

    private static void ValidateMaterialBoundaries(WorldGenerationRules rules)
    {
        var cases = new (int Height, double Temperature, double Humidity, BlockId Surface)[]
        {
            (32, -1, 0, BlockCatalog.Sand),
            (33, 0, 0, BlockCatalog.Grass),
            (60, -0.38, 0, BlockCatalog.Grass),
            (60, -0.381, 0, BlockCatalog.Snow),
            (61, -0.38, 0, BlockCatalog.Snow),
            (45, 0.18, -0.2, BlockCatalog.Grass),
            (45, 0.181, -0.1, BlockCatalog.Grass),
            (45, 0.181, -0.101, BlockCatalog.Sand),
            (105, 0, 0, BlockCatalog.Grass),
            (106, 0, 0, BlockCatalog.Stone),
            (150, 1, 0, BlockCatalog.Stone),
            (151, 1, 0, BlockCatalog.Snow),
            (151, 1, -0.2, BlockCatalog.Snow)
        };
        foreach (var sample in cases)
        {
            var plan = rules.PlanTerrainColumn(Sample(-33, -1, sample.Height, sample.Temperature, sample.Humidity));
            Require(plan.SurfaceBlock == sample.Surface, $"material boundary at {sample} matches native classification");
            Require(plan.TerrainHeight == sample.Height, "material planning never reshapes host terrain");
            Require(plan.WorldX == -33 && plan.WorldZ == -1 && plan.LocalX == 31 && plan.LocalZ == 31,
                "signed world and local coordinates are preserved");
        }
        var highland = Sample(0, 0, 45, 0, 0) with { IsLowland = false };
        Require(!rules.PlanTerrainColumn(highland).IsLowland, "host lowland classification is preserved");
    }

    private static void ValidateServerGeneration()
    {
        var rules = NativeTerrainGenerationLibrary.MaterialRulesFrom(new WorldGenerationRules());
        var sampled = FirstGeneratedBlock(in rules, 0, 0);

        Require(sampled.Block != BlockId.Air, "server can sample deterministic base terrain in memory");
        Require(sampled.Position.Y >= ChunkConstants.WorldMinY, "sampled base terrain stays above min y");
        Require(sampled.Position.Y < ChunkConstants.WorldMaxYExclusive, "sampled base terrain stays below max y");
        Require(NativeTerrainGenerationLibrary.GeneratedBlock(sampled.Position, in rules) == sampled.Block, "base terrain sampling is deterministic");
        Require(NativeTerrainGenerationLibrary.GeneratedBlock(new BlockPosition(sampled.Position.X, ChunkConstants.WorldMaxYExclusive + 1, sampled.Position.Z), in rules) == BlockId.Air, "out-of-range sampling is air");
    }

    private static void ValidateActivatorKeepsMissingWorldInMemory()
    {
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        var root = Path.Combine(Path.GetTempPath(), "octaryn-server-world-generation-probe", Guid.NewGuid().ToString("N"));
        var path = Path.Combine(root, "world_blocks.json");
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", path);

        try
        {
            using (var activator = new ModuleActivator(new ModuleRegistration()))
            {
                Require(activator.Activate(new RejectingCommandSink()) == 0, "basegame activator opens missing world");
                var blocks = activator.SnapshotBlocks();
                Require(blocks.Count == 0, "missing world keeps seed terrain out of edit storage");
            }

            Require(!File.Exists(path), "missing world does not persist generated seed terrain");
        }
        finally
        {
            Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", previousPath);
            if (Directory.Exists(root))
            {
                Directory.Delete(root, recursive: true);
            }
        }
    }

    private static void ValidateActivatorCleansGeneratedOverrides()
    {
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        var root = Path.Combine(Path.GetTempPath(), "octaryn-server-world-generation-probe", Guid.NewGuid().ToString("N"));
        var path = Path.Combine(root, "world_blocks.json");
        Directory.CreateDirectory(root);
        NativeWorldPersistenceLibrary.EnsureWorldGenerationForRoot(root);
        var rules = NativeTerrainGenerationLibrary.MaterialRulesFrom(new WorldGenerationRules());
        var generated = FirstGeneratedBlock(in rules, 0, 0);
        WorldBlockOverrideProbeFile.Save(path, new WorldBlockOverrideProbeFile
        {
            Blocks = [new WorldBlockOverrideProbeRecord(generated.Position.X, generated.Position.Y, generated.Position.Z, generated.Block.Value)]
        });
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", path);

        try
        {
            using (var activator = new ModuleActivator(new ModuleRegistration()))
            {
                Require(activator.Activate(new RejectingCommandSink()) == 0, "basegame activator cleans generated override");
                var blocks = activator.SnapshotBlocks();
                Require(blocks.Count == 0, "generated terrain override is removed from edit storage");
            }

            Require(!File.Exists(path), "generated terrain override is removed from persistence");
        }
        finally
        {
            Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", previousPath);
            if (Directory.Exists(root))
            {
                Directory.Delete(root, recursive: true);
            }
        }
    }

    private static void ValidateActivatorKeepsPersistedWorld()
    {
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        var root = Path.Combine(Path.GetTempPath(), "octaryn-server-world-generation-probe", Guid.NewGuid().ToString("N"));
        var path = Path.Combine(root, "world_blocks.json");
        Directory.CreateDirectory(root);
        NativeWorldPersistenceLibrary.EnsureWorldGenerationForRoot(root);
        WorldBlockOverrideProbeFile.Save(path, new WorldBlockOverrideProbeFile
        {
            Blocks =
            [
                new WorldBlockOverrideProbeRecord(4, 250, 4, BlockCatalog.Planks.Value),
                new WorldBlockOverrideProbeRecord(5, 250, 4, BlockCatalog.Glass.Value)
            ]
        });
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", path);

        try
        {
            using var activator = new ModuleActivator(new ModuleRegistration());
            Require(activator.Activate(new RejectingCommandSink()) == 0, "basegame activator keeps persisted world");
            var blocks = activator.SnapshotBlocks();
            Require(blocks.Count == 2, "authored edit overrides are not reseeded or erased");
        }
        finally
        {
            Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", previousPath);
            if (Directory.Exists(root))
            {
                Directory.Delete(root, recursive: true);
            }
        }
    }

    private static void ValidateManifestCapabilities()
    {
        var manifest = new ModuleRegistration().Manifest;
        Require(manifest.RequiredCapabilities.Contains(ModuleCapabilityIds.WorldgenBiomes, StringComparer.Ordinal), "manifest declares biome capability");
        Require(manifest.RequiredCapabilities.Contains(ModuleCapabilityIds.WorldgenFeatures, StringComparer.Ordinal), "manifest declares feature capability");
        Require(manifest.RequiredCapabilities.Contains(ModuleCapabilityIds.WorldgenNoise, StringComparer.Ordinal), "manifest declares noise capability");
        Require(GameModuleValidator.Validate(manifest).IsValid, "basegame manifest validates with worldgen capabilities");
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException(message);
        }
    }

    private static BlockEdit FirstGeneratedBlock(in NativeTerrainMaterialRules rules, int worldX, int worldZ)
    {
        for (var y = ChunkConstants.WorldMaxYExclusive - 1; y >= ChunkConstants.WorldMinY; y--)
        {
            var position = new BlockPosition(worldX, y, worldZ);
            var block = NativeTerrainGenerationLibrary.GeneratedBlock(position, in rules);
            if (block != BlockId.Air)
            {
                return new BlockEdit(position, block);
            }
        }

        throw new InvalidOperationException("expected generated base terrain sample");
    }

    private sealed class RejectingCommandSink : IHostCommandSink
    {
        public bool Enqueue(HostCommand command)
        {
            _ = command;
            return false;
        }
    }

    private static TerrainColumnSample Sample(
        int worldX,
        int worldZ,
        int terrainHeight,
        double temperature,
        double humidity)
    {
        return new TerrainColumnSample(
            worldX,
            worldZ,
            (worldX % ChunkConstants.Width + ChunkConstants.Width) % ChunkConstants.Width,
            (worldZ % ChunkConstants.Depth + ChunkConstants.Depth) % ChunkConstants.Depth,
            ChunkConstants.Width,
            ChunkConstants.Depth,
            terrainHeight,
            temperature,
            humidity,
            IsLowland: true);
    }
}
