using Octaryn.Server.Modules;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.Host;

internal static partial class ServerWorldBlocksProbe
{
    private static void ValidateChunkPublication()
    {
        var tracker = new ChunkPublicationTracker();
        var window = new NativeChunkViewIntent(1, 7, -2, 3, 4, 0, 0, 0, 0);
        const string path = "first/chunk_stream.json";
        Require(tracker.NeedsFullSnapshot(path, 0) && tracker.ShouldPublish(path, window, 0, false),
            "initial revision zero still requires full baseline");
        tracker.Write(path, window, 0, true, () => 1);
        Require(tracker.NeedsFullSnapshot(path, 0), "metadata-only write cannot establish full baseline");
        var calls = 0;
        try
        {
            tracker.Write<int>(path, window, 0, false, () => { calls++; throw new IOException("fixture write failure"); });
            throw new InvalidOperationException("publication failure must propagate");
        }
        catch (IOException) { }
        Require(calls == 1 && tracker.NeedsFullSnapshot(path, 0) && tracker.ShouldPublish(path, window, 0, false),
            "failed writer must retain initial retry");
        Require(tracker.Write(path, window, 0, false, () => 42) == 42, "publication returns actual writer result");
        Require(!tracker.ShouldPublish(path, window, 0, false), "unchanged successful baseline stays idle");
        Require(tracker.ShouldPublish(path, window, 0, true), "submitted commands retain existing forced-write behavior");
        Require(tracker.ShouldPublish(path, window, 1, false), "autonomous change publishes without a new command or window");
        tracker.Write(path, window, 1, true, () => 1);
        Require(tracker.NeedsFullSnapshot(path, 1), "metadata-only publication must not consume changed revision");
        try { tracker.Write<int>(path, window, 1, false, () => throw new IOException("retry")); }
        catch (IOException) { }
        Require(tracker.ShouldPublish(path, window, 1, false), "failed dirty publication remains retryable");
        ulong currentRevision = 1;
        tracker.Write(path, window, currentRevision, false, () => ++currentRevision);
        Require(tracker.NeedsFullSnapshot(path, currentRevision), "writer may acknowledge only the captured revision");
        tracker.Write(path, window, currentRevision, false, () => 1);
        Require(!tracker.ShouldPublish(path, window, currentRevision, false), "successful retry clears only published revision");
        Require(tracker.ShouldPublish(path, new(1, 8, -2, 3, 4, 0, 0, 0, 0), currentRevision, false), "epoch change publishes");
        Require(tracker.ShouldPublish(path, new(1, 7, -3, 3, 4, 0, 0, 0, 0), currentRevision, false), "signed center change publishes");
        Require(tracker.ShouldPublish(path, new(1, 7, -2, 4, 4, 0, 0, 0, 0), currentRevision, false), "z center change publishes");
        Require(tracker.ShouldPublish(path, new(1, 7, -2, 3, 2, 0, 0, 0, 0), currentRevision, false), "radius change publishes");
        Require(tracker.NeedsFullSnapshot("second/chunk_stream.json", currentRevision), "new destination requires own baseline");
        tracker.Write("second/chunk_stream.json", window, currentRevision, true, () => 1);
        Require(tracker.NeedsFullSnapshot(path, currentRevision), "metadata destination switch cannot resurrect old baseline");
        tracker.Write("second/chunk_stream.json", window, currentRevision, false, () => 1);
        Require(tracker.NeedsFullSnapshot(path, currentRevision), "full A to B to A switch requires fresh baseline");
        Require(new ChunkPublicationTracker().NeedsFullSnapshot(path, currentRevision), "new authority instance cannot inherit watermark");
        ValidateAuthoritativeBlockRevision();
        ValidateProcessPublication();
        Console.WriteLine("chunk_publication=passed revision=passed retry=passed metadata_only=passed instance_path=passed");
    }

    private static unsafe void ValidateAuthoritativeBlockRevision()
    {
        var previousPath = UseProbePersistenceFile("publication-revision");
        try
        {
            using var activator = new ModuleActivator(new BlockEditRegistration());
            Require(activator.Activate(new RejectingCommandSink()) == 0, "revision fixture activation");
            Require(activator.BlockRevision == 0, "activation without block changes has no edit revision");
            activator.Tick(Frame(1));
            var revision = activator.BlockRevision;
            Require(revision == 1, "actual module edit advances revision");
            activator.Tick(Frame(2));
            Require(activator.BlockRevision == revision, "unchanged module edit does not advance revision");
            var command = new HostCommand {
                Version = HostCommand.VersionValue, Size = HostCommand.SizeValue,
                Kind = HostCommandKind.SetBlock, A = -2, B = 3, C = 4, D = 5
            };
            Require(activator.SubmitClientCommands(&command, 1) == 0, "revision fixture queues real client edit");
            Require(activator.BlockRevision == revision, "queued edit cannot advance authority revision");
            activator.Tick(Frame(3));
            Require(activator.BlockRevision == revision + 1, "applied client edit advances revision");
            revision = activator.BlockRevision;
            Require(activator.SubmitClientCommands(&command, 1) == 0, "unchanged client edit can queue");
            activator.Tick(Frame(4));
            Require(activator.BlockRevision == revision, "unchanged client edit cannot advance revision");
            command.Kind = HostCommandKind.None;
            Require(activator.SubmitClientCommands(&command, 1) != 0, "invalid client command rejected");
            Require(activator.BlockRevision == revision, "rejected command cannot advance revision");
            Require(activator.ChunkPublication.NeedsFullSnapshot("probe", revision),
                "ordinary authority ticks do not acknowledge process snapshot publication");
        }
        finally { RestorePersistencePath(previousPath); }
    }
}
