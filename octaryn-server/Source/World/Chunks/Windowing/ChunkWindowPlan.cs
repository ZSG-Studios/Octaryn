namespace Octaryn.Server.World.Chunks;

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

internal enum ChunkWindowEventKind
{
    Load,
    Preserve,
    Unload
}
