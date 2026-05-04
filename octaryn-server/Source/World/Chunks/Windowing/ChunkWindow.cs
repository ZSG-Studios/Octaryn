namespace Octaryn.Server.World.Chunks;

internal static class ChunkWindow
{
    public static ChunkWindowPlan Plan(ChunkWindowIntent intent)
    {
        var current = WindowSet(intent.CenterChunkX, intent.CenterChunkZ, intent.Radius);
        HashSet<ChunkWindowColumn> previous = intent.HasPreviousWindow
            ? WindowSet(intent.PreviousCenterChunkX, intent.PreviousCenterChunkZ, intent.PreviousRadius)
            : [];

        List<ChunkWindowEvent> events = [];
        foreach (var chunk in current.OrderBy(chunk => chunk.X).ThenBy(chunk => chunk.Z))
        {
            events.Add(new ChunkWindowEvent(
                previous.Contains(chunk) ? ChunkWindowEventKind.Preserve : ChunkWindowEventKind.Load,
                chunk.X,
                chunk.Z));
        }

        foreach (var chunk in previous.Except(current).OrderBy(chunk => chunk.X).ThenBy(chunk => chunk.Z))
        {
            events.Add(new ChunkWindowEvent(ChunkWindowEventKind.Unload, chunk.X, chunk.Z));
        }

        return new ChunkWindowPlan(intent.Epoch, events);
    }

    private static HashSet<ChunkWindowColumn> WindowSet(int centerChunkX, int centerChunkZ, uint radius)
    {
        HashSet<ChunkWindowColumn> columns = [];
        var radiusInt = (int)radius;
        for (var chunkZ = centerChunkZ - radiusInt; chunkZ <= centerChunkZ + radiusInt; chunkZ++)
        for (var chunkX = centerChunkX - radiusInt; chunkX <= centerChunkX + radiusInt; chunkX++)
        {
            columns.Add(new ChunkWindowColumn(chunkX, chunkZ));
        }

        return columns;
    }
}

internal sealed record ChunkWindowIntent(
    ulong Epoch,
    int CenterChunkX,
    int CenterChunkZ,
    uint Radius,
    bool HasPreviousWindow,
    int PreviousCenterChunkX,
    int PreviousCenterChunkZ,
    uint PreviousRadius);

internal sealed record ChunkWindowPlan(
    ulong Epoch,
    IReadOnlyList<ChunkWindowEvent> Events)
{
    public int LoadCount => Events.Count(static @event => @event.Kind == ChunkWindowEventKind.Load);

    public int PreserveCount => Events.Count(static @event => @event.Kind == ChunkWindowEventKind.Preserve);

    public int UnloadCount => Events.Count(static @event => @event.Kind == ChunkWindowEventKind.Unload);
}

internal sealed record ChunkWindowEvent(
    ChunkWindowEventKind Kind,
    int ChunkX,
    int ChunkZ);

internal readonly record struct ChunkWindowColumn(int X, int Z);

internal enum ChunkWindowEventKind
{
    Load,
    Preserve,
    Unload
}
