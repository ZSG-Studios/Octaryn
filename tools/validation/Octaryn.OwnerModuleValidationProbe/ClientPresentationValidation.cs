using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Client.HostBridge;
using Octaryn.Client.WorldPresentation;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

internal static unsafe class ClientPresentationValidation
{
    internal static int Run()
    {
        var previous = Environment.GetEnvironmentVariable("OCTARYN_CLIENT_DISABLE_GAME_MODULES");
        Environment.SetEnvironmentVariable("OCTARYN_CLIENT_DISABLE_GAME_MODULES", "1");
        delegate* unmanaged[Cdecl]<NativeHostApi*, int> initialize = &HostExports.Initialize;
        delegate* unmanaged[Cdecl]<ServerSnapshotHeader*, int> apply = &HostExports.ApplyServerSnapshot;
        delegate* unmanaged[Cdecl]<ReplicationChange*, uint, uint*, int> drain = &HostExports.DrainPresentationUpdates;
        delegate* unmanaged[Cdecl]<void> shutdown = &HostExports.Shutdown;
        try
        {
            var api = new NativeHostApi { Version = 1, Size = NativeHostApi.SizeValue, EnqueueCommand = &Enqueue };
            var edits = stackalloc ReplicationChange[2];
            edits[0] = Edit(-4, 5, 6, 7);
            edits[1] = Edit(8, -9, 10, 0);
            var header = new ServerSnapshotHeader(0, 2, 1, 0, (ulong)edits);
            var output = stackalloc ReplicationChange[2];
            uint written = 0;
            Check(apply(&header) == -1, "apply before initialization");
            Check(initialize(&api) == 0, "initialize without module/native runtime");
            Check(apply(&header) == 0, "accept block edits");
            edits[0] = Edit(99, 99, 99, 99);
            Check(drain(null, 0, &written) == 0 && written == 0, "zero capacity preserves pending edits");
            Check(drain(output, 1, &written) == 0 && written == 1, "partial drain");
            Check(BlockReplicationChange.TryRead(in output[0], out var first) &&
                  first.Position == new BlockPosition(-4, 5, 6) && first.Block == new BlockId(7), "copied signed coordinates");
            Check(drain(output, 2, &written) == 0 && written == 1, "remaining edit");
            Check(BlockReplicationChange.TryRead(in output[0], out var second) && second.Block == new BlockId(0), "preserve edited air");
            Check(drain(output, 2, &written) == 0 && written == 0, "empty queue");

            edits[1] = new ReplicationChange(999, 1, 0, 0);
            Check(apply(&header) == -2, "reject unsupported kind");
            Check(drain(output, 2, &written) == 0 && written == 0, "invalid batch is atomic");
            var nullChanges = new ServerSnapshotHeader(0, 1, 1, 0, 0);
            Check(apply(&nullChanges) == -2, "reject missing changes");
            var unsupportedIds = new ServerSnapshotHeader(1, 0, 1, 0, 0);
            Check(apply(&unsupportedIds) == -2, "reject unsupported entity list");
            Check(drain(null, 1, &written) == -1 && drain(output, uint.MaxValue, &written) == -1, "reject invalid drain span");

            var full = new ReplicationChange[BlockUpdateQueue.Capacity];
            Array.Fill(full, Edit(1, 2, 3, 4));
            fixed (ReplicationChange* pointer = full)
            {
                var fullHeader = new ServerSnapshotHeader(0, (uint)full.Length, 2, 0, (ulong)pointer);
                Check(apply(&fullHeader) == 0, "bounded capacity");
                edits[1] = Edit(8, -9, 10, 0);
                Check(apply(&header) == -3, "explicit backpressure");
                Check(drain(pointer, (uint)full.Length, &written) == 0 && written == full.Length, "no edits lost to rejected batch");
            }
            Check(apply(&header) == 0 && initialize(&api) == 0, "reinitialize with pending edits");
            Check(drain(output, 2, &written) == 0 && written == 0, "reinitialize clears pending edits");
            Check(apply(&header) == 0, "enqueue before shutdown");
            shutdown();
            Check(apply(&header) == -1, "shutdown rejects snapshots");
            Check(initialize(&api) == 0 && drain(output, 2, &written) == 0 && written == 0, "restart clears pending edits");
            Console.WriteLine("Client presentation ABI PASS: copied edits, air, partial drains, atomic rejection, bounds, reinitialize and shutdown.");
            return 0;
        }
        finally
        {
            shutdown();
            Environment.SetEnvironmentVariable("OCTARYN_CLIENT_DISABLE_GAME_MODULES", previous);
        }
    }

    private static ReplicationChange Edit(int x, int y, int z, ushort block) =>
        new BlockReplicationChange(new BlockPosition(x, y, z), new BlockId(block)).ToReplicationChange(1);

    private static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int Enqueue(HostCommand* command) => command is null ? 0 : 1;
}
