namespace Octaryn.Server.World.Chunks;

internal static class ServerChunkWindow
{
    public static ServerChunkWindowPlan Plan(ServerChunkWindowIntent intent)
    {
        var current = WindowSet(intent.CenterChunkX, intent.CenterChunkZ, intent.Radius);
        HashSet<ServerChunkWindowColumn> previous = intent.HasPreviousWindow
            ? WindowSet(intent.PreviousCenterChunkX, intent.PreviousCenterChunkZ, intent.PreviousRadius)
            : [];

        List<ServerChunkWindowEvent> events = [];
        foreach (var chunk in current.OrderBy(chunk => chunk.X).ThenBy(chunk => chunk.Z))
        {
            events.Add(new ServerChunkWindowEvent(
                previous.Contains(chunk) ? ServerChunkWindowEventKind.Preserve : ServerChunkWindowEventKind.Load,
                chunk.X,
                chunk.Z));
        }

        foreach (var chunk in previous.Except(current).OrderBy(chunk => chunk.X).ThenBy(chunk => chunk.Z))
        {
            events.Add(new ServerChunkWindowEvent(ServerChunkWindowEventKind.Unload, chunk.X, chunk.Z));
        }

        return new ServerChunkWindowPlan(intent.Epoch, events);
    }

    private static HashSet<ServerChunkWindowColumn> WindowSet(int centerChunkX, int centerChunkZ, uint radius)
    {
        HashSet<ServerChunkWindowColumn> columns = [];
        var radiusInt = (int)radius;
        for (var chunkZ = centerChunkZ - radiusInt; chunkZ <= centerChunkZ + radiusInt; chunkZ++)
        for (var chunkX = centerChunkX - radiusInt; chunkX <= centerChunkX + radiusInt; chunkX++)
        {
            columns.Add(new ServerChunkWindowColumn(chunkX, chunkZ));
        }

        return columns;
    }
}

internal sealed record ServerChunkWindowIntent(
    ulong Epoch,
    int CenterChunkX,
    int CenterChunkZ,
    uint Radius,
    bool HasPreviousWindow,
    int PreviousCenterChunkX,
    int PreviousCenterChunkZ,
    uint PreviousRadius);

internal sealed record ServerChunkWindowPlan(
    ulong Epoch,
    IReadOnlyList<ServerChunkWindowEvent> Events)
{
    public int LoadCount => Events.Count(static @event => @event.Kind == ServerChunkWindowEventKind.Load);

    public int PreserveCount => Events.Count(static @event => @event.Kind == ServerChunkWindowEventKind.Preserve);

    public int UnloadCount => Events.Count(static @event => @event.Kind == ServerChunkWindowEventKind.Unload);
}

internal sealed record ServerChunkWindowEvent(
    ServerChunkWindowEventKind Kind,
    int ChunkX,
    int ChunkZ);

internal readonly record struct ServerChunkWindowColumn(int X, int Z);

internal enum ServerChunkWindowEventKind
{
    Load,
    Preserve,
    Unload
}
