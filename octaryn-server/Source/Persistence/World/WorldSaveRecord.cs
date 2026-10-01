namespace Octaryn.Server.Persistence.World;

internal sealed record SavedPlayerPose(float X, float Y, float Z, float Pitch, float Yaw);
internal sealed record SavedWorldClock(uint Version, ulong DayIndex, double SecondsOfDay, double SpeedMultiplier);
internal sealed record WorldSaveRecord(
    int Version, string ModuleId, string CompatibilityId, ulong Generation, uint SessionId,
    SavedPlayerPose Player, SavedWorldClock Clock, byte[] ModuleState);
