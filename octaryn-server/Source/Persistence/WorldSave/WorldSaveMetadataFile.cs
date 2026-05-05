using Octaryn.Server.Persistence.WorldBlocks;

namespace Octaryn.Server.Persistence.WorldSave;

internal static class WorldSaveMetadataFile
{
    private static NativePersistenceWorldMetadata ToNative(WorldSaveMetadata metadata)
    {
        return new NativePersistenceWorldMetadata(
            metadata.SaveExists ? 1u : 0u,
            metadata.HasWorldTime ? 1u : 0u,
            metadata.HasPlayerData ? 1u : 0u,
            metadata.HasWorldData ? 1u : 0u,
            metadata.PlayerCount,
            metadata.ChunkOverrideCount);
    }

    private static WorldSaveMetadata FromNative(NativePersistenceWorldMetadata metadata)
    {
        return new WorldSaveMetadata(
            metadata.SaveExists != 0u,
            metadata.HasWorldTime != 0u,
            metadata.HasPlayerData != 0u,
            metadata.HasWorldData != 0u,
            metadata.PlayerCount,
            metadata.ChunkOverrideCount);
    }

    public static bool TryLoad(string path, out WorldSaveMetadata metadata)
    {
        metadata = default;
        if (!NativeWorldPersistenceLibrary.TryReadWorldMetadataFile(path, out var nativeMetadata))
        {
            return false;
        }

        metadata = FromNative(nativeMetadata);
        return true;
    }

    public static void Save(string path, WorldSaveMetadata metadata)
    {
        NativeWorldPersistenceLibrary.WriteWorldMetadataFile(path, ToNative(metadata));
    }
}
