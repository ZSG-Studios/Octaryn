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

return ServerWorldGenerationProbe.Run();

internal static class ServerWorldGenerationProbe
{
    public static int Run()
    {
        ValidateBasegameRules();
        ValidateServerGeneration();
        ValidateActivatorGenerationPath();
        ValidateActivatorSeedsMissingWorld();
        ValidateActivatorSeedsSingleBlockWorld();
        ValidateActivatorKeepsPersistedWorld();
        ValidateManifestCapabilities();
        return 0;
    }

    private static void ValidateBasegameRules()
    {
        var rules = new WorldGenerationRules();
        Require(rules.WaterHeight == 30, "water height matches old worldgen");
        Require(rules.WaterBlock == BlockCatalog.WaterSource, "water fill uses stable basegame water block");

        var sand = rules.PlanTerrainColumn(Sample(0, 0, 0.0f, -1.0f, -1.0f));
        Require(sand.TerrainHeight == 18, "lowland noise adjusts old low terrain");
        Require(sand.SurfaceBlock == BlockCatalog.Sand, "low terrain uses sand surface");
        Require(sand.FillBlock == BlockCatalog.Sand, "low terrain uses sand fill");

        var grass = rules.PlanTerrainColumn(Sample(1, 0, 0.3f, 0.0f, -1.0f));
        Require(grass.SurfaceBlock == BlockCatalog.Grass, "mid lowland terrain uses grass surface");
        Require(grass.FillBlock == BlockCatalog.Dirt, "mid lowland terrain uses dirt fill");
        Require(grass.HasGrassSurface, "grass terrain accepts flora");

        var stone = rules.PlanTerrainColumn(Sample(2, 0, 0.7f, 0.0f, 0.0f));
        Require(stone.SurfaceBlock == BlockCatalog.Stone, "high terrain uses stone surface");
        Require(stone.FillBlock == BlockCatalog.Stone, "high terrain uses stone fill");

        var snow = rules.PlanTerrainColumn(Sample(3, 0, 3.0f, 0.0f, 0.0f));
        Require(snow.SurfaceBlock == BlockCatalog.Snow, "peak terrain uses snow surface");
        Require(snow.FillBlock == BlockCatalog.Stone, "peak terrain uses stone fill");

        var featureColumn = rules.PlanTerrainColumn(Sample(4, 0, 0.0f, 0.0f, 2.0f));
        var featureBlocks = new List<BlockEdit>();
        rules.AddFeatureBlocks(featureColumn, 0.05f, featureBlocks);
        Require(featureBlocks.Count == 1 && featureBlocks[0].Block == BlockCatalog.Gardenia, "flower threshold uses old flower selection order");

        featureBlocks.Clear();
        rules.AddFeatureBlocks(featureColumn, 0.2f, featureBlocks);
        Require(featureBlocks.Count == 1 && featureBlocks[0].Block == BlockCatalog.Bush, "bush threshold emits bush");

        featureBlocks.Clear();
        var treeColumn = featureColumn with { LocalX = 3, LocalZ = 3, DecorationY = 40 };
        rules.AddFeatureBlocks(treeColumn, 0.8f, featureBlocks);
        Require(featureBlocks.Count == 21, "tree threshold emits trunk and leaves");
        Require(featureBlocks.Count(block => block.Block == BlockCatalog.Log) == 4, "tree trunk height follows old rule");
        Require(featureBlocks.Count(block => block.Block == BlockCatalog.Leaves) == 17, "tree leaves follow old canopy rule");
    }

    private static void ValidateServerGeneration()
    {
        var generator = new TerrainGenerator(new WorldGenerationRules());
        var blocks = generator.GenerateChunkColumn(0, 0);

        Require(blocks.Count > ChunkConstants.Width * ChunkConstants.Depth, "generation emits terrain blocks");
        Require(blocks.All(block => block.Position.Y >= ChunkConstants.WorldMinY), "generated blocks stay above min y");
        Require(blocks.All(block => block.Position.Y < ChunkConstants.WorldMaxYExclusive), "generated blocks stay below max y");
        Require(blocks.Any(block => block.Position.Y == ChunkConstants.WorldMinY), "generation fills centered world floor");
        Require(blocks.Any(block => block.Position.Y < 0), "generation fills below origin in centered world");
        Require(blocks.Any(block => IsTerrainSurfaceBlock(block.Block)), "generation emits terrain surface blocks");
        var waterBlocks = new TerrainGenerator(new FixedLowlandRules()).GenerateChunkColumn(0, 0);
        Require(waterBlocks.Any(block => block.Block == BlockCatalog.WaterSource), "generation emits water where terrain is below water height");

        var repeated = generator.GenerateChunkColumn(0, 0);
        Require(blocks.SequenceEqual(repeated), "generation is deterministic for the same chunk column");

        var neighbor = generator.GenerateChunkColumn(ChunkConstants.Width, 0);
        Require(neighbor.Any(block => block.Position.X >= ChunkConstants.Width), "neighbor origin maps to world x");
        Require(!blocks.SequenceEqual(neighbor), "neighbor chunk has distinct terrain");
    }

    private static void ValidateActivatorGenerationPath()
    {
        using var activator = new ModuleActivator(new ModuleRegistration());
        var blocks = activator.GenerateTerrainChunkColumn(0, 0);
        Require(blocks.Count > 0, "server activator exposes generation for modules with worldgen rules");
    }

    private static void ValidateActivatorSeedsMissingWorld()
    {
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        var root = Path.Combine(Path.GetTempPath(), "octaryn-server-world-generation-probe", Guid.NewGuid().ToString("N"));
        var path = Path.Combine(root, "world_blocks.json");
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", path);

        try
        {
            using (var activator = new ModuleActivator(new ModuleRegistration()))
            {
                Require(activator.Activate(new RejectingCommandSink()) == 0, "basegame activator seeds missing world");
                var blocks = activator.SnapshotBlocks();
                Require(blocks.Count > ChunkConstants.Width * ChunkConstants.Depth, "seeded basegame world has more than one visible layer");
                Require(blocks.Any(block => block.Position.Y < 0), "seeded basegame world fills below origin");
                Require(blocks.Any(block => IsTerrainSurfaceBlock(block.Block)), "seeded basegame world includes basegame surface blocks");
            }

            Require(File.Exists(path), "seeded basegame world is persisted");
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

    private static void ValidateActivatorSeedsSingleBlockWorld()
    {
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        var root = Path.Combine(Path.GetTempPath(), "octaryn-server-world-generation-probe", Guid.NewGuid().ToString("N"));
        var path = Path.Combine(root, "world_blocks.json");
        Directory.CreateDirectory(root);
        WorldBlockOverrideFile.Save(path, new WorldBlockOverrideFile
        {
            Blocks = [new WorldBlockOverrideRecord(0, 0, 0, BlockCatalog.Grass.Value)]
        });
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", path);

        try
        {
            using var activator = new ModuleActivator(new ModuleRegistration());
            Require(activator.Activate(new RejectingCommandSink()) == 0, "basegame activator seeds single-block world");
            var blocks = activator.SnapshotBlocks();
            Require(blocks.Count > ChunkConstants.Width * ChunkConstants.Depth, "single-block world expands to generated terrain");
            Require(blocks.Any(block => block.Position.Y < 0), "single-block world gains centered terrain below origin");
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
        WorldBlockOverrideFile.Save(path, new WorldBlockOverrideFile
        {
            Blocks =
            [
                new WorldBlockOverrideRecord(4, 0, 4, BlockCatalog.Grass.Value),
                new WorldBlockOverrideRecord(5, 0, 4, BlockCatalog.Dirt.Value)
            ]
        });
        Environment.SetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH", path);

        try
        {
            using var activator = new ModuleActivator(new ModuleRegistration());
            Require(activator.Activate(new RejectingCommandSink()) == 0, "basegame activator keeps persisted world");
            var blocks = activator.SnapshotBlocks();
            Require(blocks.Count == 2, "multi-block persisted world is not reseeded");
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

    private static bool IsTerrainSurfaceBlock(BlockId block)
    {
        return block == BlockCatalog.Grass ||
            block == BlockCatalog.Sand ||
            block == BlockCatalog.Snow ||
            block == BlockCatalog.Stone;
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
        float heightNoise,
        float lowlandNoise,
        float biomeNoise)
    {
        return new TerrainColumnSample(
            worldX,
            worldZ,
            worldX,
            worldZ,
            ChunkConstants.Width,
            ChunkConstants.Depth,
            ChunkConstants.WorldMaxYExclusive - 1,
            heightNoise,
            lowlandNoise,
            biomeNoise);
    }

    private sealed class FixedLowlandRules : IWorldGenerationRules
    {
        public int WaterHeight => 30;

        public BlockId WaterBlock => BlockCatalog.WaterSource;

        public TerrainColumnPlan PlanTerrainColumn(TerrainColumnSample sample)
        {
            return new TerrainColumnPlan(
                sample.WorldX,
                sample.WorldZ,
                sample.LocalX,
                sample.LocalZ,
                sample.LocalWidth,
                sample.LocalDepth,
                TerrainHeight: 18,
                DecorationY: 30,
                SurfaceBlock: BlockCatalog.Sand,
                FillBlock: BlockCatalog.Sand,
                IsLowland: true,
                HasGrassSurface: false);
        }

        public void AddFeatureBlocks(TerrainColumnPlan column, float plantNoise, ICollection<BlockEdit> blocks)
        {
            _ = column;
            _ = plantNoise;
            _ = blocks;
        }
    }
}
