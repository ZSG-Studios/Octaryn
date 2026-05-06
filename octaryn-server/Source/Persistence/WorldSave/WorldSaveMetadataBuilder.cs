using Octaryn.Server.Persistence.WorldBlocks;

namespace Octaryn.Server.Persistence.WorldSave;

internal static class WorldSaveMetadataBuilder
{
    public static WorldSaveMetadata Build(string worldRoot)
    {
        var metadata = NativeWorldPersistenceLibrary.BuildWorldMetadata(worldRoot);
        return new WorldSaveMetadata(
            metadata.SaveExists != 0u,
            metadata.HasWorldTime != 0u,
            metadata.HasPlayerData != 0u,
            metadata.HasWorldData != 0u,
            metadata.PlayerCount,
            metadata.ChunkOverrideCount);
    }
}
