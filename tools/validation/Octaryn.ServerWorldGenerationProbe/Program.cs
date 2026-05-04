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
        ValidateActivatorKeepsMissingWorldInMemory();
        ValidateActivatorCleansGeneratedOverrides();
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
        var sampled = FirstGeneratedBlock(generator, 0, 0);

        Require(sampled.Block != BlockId.Air, "server can sample deterministic base terrain in memory");
        Require(sampled.Position.Y >= ChunkConstants.WorldMinY, "sampled base terrain stays above min y");
        Require(sampled.Position.Y < ChunkConstants.WorldMaxYExclusive, "sampled base terrain stays below max y");
        Require(generator.GetGeneratedBlock(sampled.Position) == sampled.Block, "base terrain sampling is deterministic");
        Require(generator.GetGeneratedBlock(new BlockPosition(sampled.Position.X, ChunkConstants.WorldMaxYExclusive + 1, sampled.Position.Z)) == BlockId.Air, "out-of-range sampling is air");

        var fixedLowland = new TerrainGenerator(new FixedLowlandRules());
        Require(fixedLowland.GetGeneratedBlock(new BlockPosition(0, 29, 0)) == BlockCatalog.WaterSource, "server samples water above low terrain");
        Require(fixedLowland.GetGeneratedBlock(new BlockPosition(0, 18, 0)) == BlockCatalog.Sand, "server samples low terrain surface");
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
        var generated = FirstGeneratedBlock(new TerrainGenerator(new WorldGenerationRules()), 0, 0);
        WorldBlockOverrideFile.Save(path, new WorldBlockOverrideFile
        {
            Blocks = [new WorldBlockOverrideRecord(generated.Position.X, generated.Position.Y, generated.Position.Z, generated.Block.Value)]
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
        WorldBlockOverrideFile.Save(path, new WorldBlockOverrideFile
        {
            Blocks =
            [
                new WorldBlockOverrideRecord(4, 250, 4, BlockCatalog.Planks.Value),
                new WorldBlockOverrideRecord(5, 250, 4, BlockCatalog.Glass.Value)
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

    private static BlockEdit FirstGeneratedBlock(TerrainGenerator generator, int worldX, int worldZ)
    {
        for (var y = ChunkConstants.WorldMaxYExclusive - 1; y >= ChunkConstants.WorldMinY; y--)
        {
            var position = new BlockPosition(worldX, y, worldZ);
            var block = generator.GetGeneratedBlock(position);
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
