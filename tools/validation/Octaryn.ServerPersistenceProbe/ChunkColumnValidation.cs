using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldSave;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

internal static partial class ServerPersistenceProbe
{
    private static void ValidateChunkColumnOverrideFiles()
    {
        var root = ResetProbeDirectory("chunk-columns");
        var edits = new[]
        {
            new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(5)),
            new BlockEdit(new BlockPosition(-1, 2, 31), new BlockId(6)),
            new BlockEdit(new BlockPosition(32, 3, 0), new BlockId(7))
        };

        ChunkColumnProbeFiles.SaveEdits(root, edits);
        var negativePath = ChunkColumnProbeFiles.PathFor(root, -32, 0);
        var originPath = ChunkColumnProbeFiles.PathFor(root, 0, 0);
        var positivePath = ChunkColumnProbeFiles.PathFor(root, 32, 0);
        Require(File.Exists(negativePath), "negative chunk column file written");
        Require(File.Exists(originPath), "origin chunk column file written");
        Require(File.Exists(positivePath), "positive chunk column file written");
        Require(ChunkColumnOverrideProbeFile.TryLoad(originPath, out var originFile), "chunk column file load");
        Require(originFile.Version == 2, "chunk column file uses old current version");
        Require(originFile.Cx == 0 && originFile.Cz == 0, "chunk column origin stored");
        Require(originFile.Blocks.Count == 1, "chunk column blocks grouped");

        var json = File.ReadAllText(originPath);
        Require(json.Contains("\"version\": 2", StringComparison.Ordinal), "chunk column json version");
        Require(json.Contains("\"cx\": 0", StringComparison.Ordinal), "chunk column json cx");
        Require(json.Contains("\"bx\": 10", StringComparison.Ordinal), "chunk column json block x");

        var loadedEdits = ChunkColumnProbeFiles.LoadEdits(root);
        Require(loadedEdits.Count == 3, "chunk column load count");
        Require(loadedEdits[0].Position == new BlockPosition(-1, 2, 31), "chunk column load sorted negative");
        Require(ChunkColumnProbeFiles.CountFiles(root) == 3, "chunk column file count");
        Require(ChunkColumnProbeFiles.CountBlocks(root) == 3, "chunk column block count");

        File.WriteAllText(originPath, json.Replace("\"version\": 2", "\"version\": 99", StringComparison.Ordinal));
        Require(!ChunkColumnOverrideProbeFile.TryLoad(originPath, out _), "unknown chunk column version rejected");

        ValidateLegacyChunkColumnOverrideMigration();

        ChunkColumnProbeFiles.SaveEdits(root, [edits[0]]);
        Require(!File.Exists(negativePath), "stale negative chunk column removed");
        Require(!File.Exists(positivePath), "stale positive chunk column removed");
        Require(File.Exists(originPath), "remaining chunk column rewritten");

        var worldBlocksPath = Path.Combine(root, "world_blocks.json");
        WorldBlockOverrideProbeFile.Save(
            worldBlocksPath,
            WorldBlockOverrideProbeFile.FromEdits([new BlockEdit(new BlockPosition(10, 1, 2), new BlockId(99))]));
        using var persistence = new WorldBlockPersistence(worldBlocksPath);
        var loadedStore = new BlockStore();
        persistence.Load(loadedStore);
        Require(loadedStore.GetBlock(new BlockPosition(10, 1, 2)).Value == 99, "newer aggregate file preferred over stale chunk columns");

        ChunkColumnProbeFiles.SaveEdits(root, [edits[0]]);
        loadedStore = new BlockStore();
        persistence.Load(loadedStore);
        Require(loadedStore.GetBlock(new BlockPosition(10, 1, 2)).Value == 5, "current chunk columns preferred over aggregate file");

        loadedStore.SetBlock(new BlockEdit(new BlockPosition(64, 4, 0), new BlockId(8)));
        persistence.MarkDirty();
        persistence.SaveIfDirty(loadedStore);
        Require(WorldBlockOverrideProbeFile.TryLoad(worldBlocksPath, out var aggregate), "aggregate override saved");
        Require(aggregate.Blocks.Count == 2, "aggregate override mirrors chunk columns");
        Require(File.Exists(ChunkColumnProbeFiles.PathFor(root, 64, 0)), "new chunk column saved on dirty flush");
    }

    private static void ValidateLegacyChunkColumnOverrideMigration()
    {
        var root = ResetProbeDirectory("chunk-column-legacy");
        var localPath = ChunkColumnProbeFiles.PathFor(root, 64, 0);
        ChunkColumnOverrideProbeFile.Save(
            localPath,
            new ChunkColumnOverrideFile
            {
                Version = 1,
                Cx = 64,
                Cz = 0,
                Blocks = [new ChunkColumnBlockOverrideRecord(1, 2, 3, 10)]
            });
        Require(ChunkColumnOverrideProbeFile.TryLoad(localPath, out var localFile), "legacy local chunk column migrates");
        var localEdit = ChunkColumnOverrideProbeFile.ToEdits(localFile).Single();
        Require(localFile.Version == 2, "legacy local chunk column upgrades version");
        Require(localEdit.Position == new BlockPosition(65, 2, 3), "legacy local coordinates become world coordinates");

        var worldPath = ChunkColumnProbeFiles.PathFor(root, 64, 64);
        ChunkColumnOverrideProbeFile.Save(
            worldPath,
            new ChunkColumnOverrideFile
            {
                Version = 1,
                Cx = 64,
                Cz = 64,
                Blocks = [new ChunkColumnBlockOverrideRecord(65, 2, 66, 11)]
            });
        Require(ChunkColumnOverrideProbeFile.TryLoad(worldPath, out var worldFile), "legacy world chunk column migrates");
        var worldEdit = ChunkColumnOverrideProbeFile.ToEdits(worldFile).Single();
        Require(worldEdit.Position == new BlockPosition(65, 2, 66), "legacy world coordinates stay unchanged");

        var ambiguousPath = ChunkColumnProbeFiles.PathFor(root, 0, 0);
        ChunkColumnOverrideProbeFile.Save(
            ambiguousPath,
            new ChunkColumnOverrideFile
            {
                Version = 1,
                Cx = 0,
                Cz = 0,
                Blocks = [new ChunkColumnBlockOverrideRecord(1, 2, 3, 12)]
            });
        Require(!ChunkColumnOverrideProbeFile.TryLoad(ambiguousPath, out _), "ambiguous legacy chunk column is rejected");
    }
}
