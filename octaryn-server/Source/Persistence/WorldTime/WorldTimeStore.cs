using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.World.Time;

namespace Octaryn.Server.Persistence.WorldTime;

internal static class WorldTimeStore
{
    public static bool TryLoad(string path, out WorldTimeBlob blob)
    {
        blob = default;
        if (!NativeWorldPersistenceLibrary.TryReadWorldTimeFile(path, out var state))
        {
            return false;
        }

        if (state.Version != WorldTimeBlob.CurrentVersion)
        {
            return false;
        }

        blob = new WorldTimeBlob(state.Version, state.DayIndex, state.SecondsOfDay);
        return true;
    }

    public static void Save(string path, WorldTimeBlob blob)
    {
        NativeWorldPersistenceLibrary.WriteWorldTimeFile(
            path,
            new NativePersistenceWorldTimeState(blob.Version, blob.DayIndex, blob.SecondsOfDay));
    }
}
