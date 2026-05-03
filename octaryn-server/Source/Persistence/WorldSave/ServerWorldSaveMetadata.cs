namespace Octaryn.Server.Persistence.WorldSave;

internal readonly record struct ServerWorldSaveMetadata(
    bool SaveExists,
    bool HasWorldTime,
    bool HasPlayerData,
    bool HasWorldData,
    int PlayerCount,
    int ChunkOverrideCount);
