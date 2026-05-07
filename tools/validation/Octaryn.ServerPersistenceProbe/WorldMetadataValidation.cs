using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.WorldSave;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Time;
using Octaryn.Shared.World;

internal static partial class ServerPersistenceProbe
{
    private static void ValidateWorldSaveMetadata()
    {
        var root = ResetProbeDirectory("world-metadata");
        var emptyMetadata = WorldSaveMetadataBuilder.Build(root);
        Require(!emptyMetadata.SaveExists, "empty metadata has no save");
        Require(emptyMetadata.PlayerCount == 0, "empty metadata player count");
        Require(emptyMetadata.ChunkOverrideCount == 0, "empty metadata chunk count");

        SaveWorldTime(Path.Combine(root, "world_time.json"), new WorldTimeBlob(1, 2, 30.5));

        var players = new PlayerPersistence(root);
        players.Save(1, new PlayerSaveState(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, new BlockId(6)));
        players.Save(2, new PlayerSaveState(6.0f, 7.0f, 8.0f, 9.0f, 10.0f, new BlockId(11)));
        File.WriteAllText(Path.Combine(root, "player_invalid.json"), "{}");

        WorldBlockOverrideProbeFile.Save(
            Path.Combine(root, "world_blocks.json"),
            WorldBlockOverrideProbeFile.FromEdits([
                new BlockEdit(new BlockPosition(-1, 2, 3), new BlockId(12)),
                new BlockEdit(new BlockPosition(1, 2, 3), new BlockId(4)),
                new BlockEdit(new BlockPosition(5, 6, 7), new BlockId(8)),
                new BlockEdit(new BlockPosition(32, 6, 7), new BlockId(9))
            ]));

        var metadata = WorldSaveMetadataBuilder.Build(root);
        Require(metadata.SaveExists, "metadata detects save");
        Require(metadata.HasWorldTime, "metadata detects world time");
        Require(metadata.HasPlayerData, "metadata detects player data");
        Require(metadata.HasWorldData, "metadata detects world data");
        Require(metadata.PlayerCount == 2, "metadata counts valid player saves");
        Require(metadata.ChunkOverrideCount == 3, "metadata counts unique aggregate chunk overrides");

        var metadataPath = NativeWorldPersistenceLibrary.WorldMetadataPathForRoot(root);
        SaveWorldMetadata(metadataPath, metadata);
        Require(TryLoadWorldMetadata(metadataPath, out var loaded), "metadata file load");
        Require(loaded == metadata, "metadata file round trip");

        var json = File.ReadAllText(metadataPath);
        Require(json.Contains("\"save_exists\": true", StringComparison.Ordinal), "metadata json save flag");
        Require(json.Contains("\"chunk_override_count\": 3", StringComparison.Ordinal), "metadata json chunk count");

        File.WriteAllText(metadataPath, json.Replace("\"version\": 1", "\"version\": 99", StringComparison.Ordinal));
        Require(!TryLoadWorldMetadata(metadataPath, out _), "unknown metadata version rejected");

        var chunkOnlyRoot = ResetProbeDirectory("world-metadata-chunks");
        ChunkColumnProbeFiles.SaveEdits(
            chunkOnlyRoot,
            [
                new BlockEdit(new BlockPosition(0, 0, 0), new BlockId(1)),
                new BlockEdit(new BlockPosition(32, 0, 0), new BlockId(2))
            ]);
        var chunkOnlyMetadata = WorldSaveMetadataBuilder.Build(chunkOnlyRoot);
        Require(chunkOnlyMetadata.SaveExists, "metadata detects chunk-only save");
        Require(chunkOnlyMetadata.HasWorldData, "metadata detects chunk-only world data");
        Require(chunkOnlyMetadata.ChunkOverrideCount == 2, "metadata counts chunk-only file overrides");
    }
}
