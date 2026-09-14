using System.Runtime.InteropServices;
using System.Security.Cryptography;
using Octaryn.Server.Modules;
using Octaryn.Server.World.Items;

internal static partial class ServerWorldBlocksProbe
{
    private static unsafe void ValidateWorldItems()
    {
        Require(!WorldItemsProcess.IsTransientFileContention(new InvalidDataException("bad schema")) &&
            !WorldItemsProcess.IsTransientFileContention(new InvalidOperationException("logic failure")),
            "item contention policy does not hide corruption or logic errors");
        var previous = UseProbePersistenceFile("world-items");
        var root = Path.GetDirectoryName(Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH"))!;
        var save = Path.Combine(root, "world_items.bin");
        var snapshot = Path.Combine(root, "runtime/world_items.snapshot");
        var intent = Path.Combine(root, "runtime/world_items.intent");
        try
        {
            using var module = new ModuleActivator(new BlockEditRegistration(), BlockPublicationMode.ProcessSnapshots);
            Require(module.Activate(new RejectingCommandSink()) == 0, "world items module activation");
            var block = Enumerable.Range(1, 50).Select(x => (ushort)x).First(module.IsItemBlockPlaceable);
            using (var items = new WorldItemsProcess(module, root))
            {
                WriteItemIntent(intent, 1, 0, block, 16);
                items.Step(.02);
                var state = ReadItemState(snapshot);
                Require(state.LastCommand == 1 && state.ReceiptResult == 1 && state.ItemCount == 1,
                    "actual process accepts counted catalog toss");
                Require(ReadItemSave(save).LastCommand == 1, "receipt follows durable save");
                items.Step(.06);
                Require(ReadItemState(snapshot).ItemCount == 1, "same process intent cannot replay spawn");
                if (OperatingSystem.IsWindows())
                {
                    WriteItemIntent(intent, 2, 0, block, 7);
                    var failed = false;
                    using (var held = new FileStream(save, FileMode.Open, FileAccess.Read, FileShare.Read))
                    {
                        try { items.Step(.2); }
                        catch (Exception error) when (WorldItemsProcess.IsTransientFileContention(error)) { failed = true; }
                        Require(failed && ReadItemState(snapshot).LastCommand == 1,
                            "locked durable save withholds new receipt and snapshot");
                    }
                    items.Step(.24);
                    Require(ReadItemState(snapshot).LastCommand == 2 && ReadItemSave(save).LastCommand == 2,
                        "actual writer retries before publishing accepted command");
                }
            }
            var saved = ReadItemSave(save);
            var command = saved.LastCommand;
            using (var reopened = new WorldItemsProcess(module, root))
            {
                reopened.Step(.01);
                Require(ReadItemState(snapshot).LastCommand == command,
                    "restart retains command replay watermark");
            }
            // Seed a valid authoritative fixture directly at the actual local player's feet.
            saved = ReadItemSave(save);
            var player = module.SnapshotPlayer();
            byte* bytes = saved.Items;
            {
                var position = (float*)(bytes + 16);
                position[0] = player.X;position[1] = player.Y - 1.3f;position[2] = player.Z;
                position[3] = position[4] = position[5] = 0;
                ((double*)(bytes + 40))[1] = 0;
            }
            WriteItemSave(save, saved);
            using (var pickup = new WorldItemsProcess(module, root))
            {
                if (OperatingSystem.IsWindows())
                {
                    var failed = false;
                    using (var held = new FileStream(save, FileMode.Open, FileAccess.Read, FileShare.Read))
                    {
                        try { pickup.Step(.01); }
                        catch (Exception error) when (WorldItemsProcess.IsTransientFileContention(error)) { failed = true; }
                        Require(failed && ReadItemState(snapshot).GrantCount == 0,
                            "failed pickup persistence cannot expose a credit");
                    }
                }
                pickup.Step(.05);
                var state = ReadItemState(snapshot);
                Require(state.GrantCount == 1 && state.NextGrant == 2,
                    "actual authority proximity removes entity and persists pickup grant");
                Require(ReadItemSave(save).GrantCount == 1, "pickup grant saved before publication");
            }
            using (var restart = new WorldItemsProcess(module, root))
            {
                Require(ReadItemState(snapshot).GrantCount == 1, "unacknowledged pickup survives restart");
                WriteItemIntent(intent, command, 2, block, 16);restart.Step(.01);
                Require(ReadItemState(snapshot).GrantCount == 1, "future grant acknowledgement rejected");
                WriteItemIntent(intent, command, 1, block, 16);restart.Step(.05);
                Require(ReadItemState(snapshot).GrantCount == 0 && ReadItemSave(save).AcknowledgedGrant == 1,
                    "oldest grant acknowledgement durable and removes exactly once");
            }
            Console.WriteLine("world_items_process=passed counted_toss replay save_retry restart pickup_grant ordered_ack");
        }
        finally { RestorePersistencePath(previous); }
    }

    private static void WriteItemIntent(string path, ulong command, ulong ack, uint block, uint count)
    {
        using var bytes = new MemoryStream();using var writer = new BinaryWriter(bytes);
        writer.Write(1u);writer.Write(32u);writer.Write(command);writer.Write(ack);writer.Write(block);writer.Write(count);
        WorldItemsProcess.AtomicWrite(path, bytes.ToArray(), true);
    }
    private static unsafe ItemState ReadItemState(string path)
    {
        var bytes = WorldItemsProcess.Read(path, sizeof(ItemState));
        Require(bytes.Length == sizeof(ItemState), "complete item snapshot size");
        fixed (byte* pointer = bytes) return *(ItemState*)pointer;
    }
    private static unsafe ItemState ReadItemSave(string path)
    {
        var bytes = WorldItemsProcess.Read(path, sizeof(ItemState) + 32);
        Require(bytes.Length == sizeof(ItemState) + 32 &&
            SHA256.HashData(bytes.AsSpan(32)).AsSpan().SequenceEqual(bytes.AsSpan(0, 32)), "item save checksum");
        fixed (byte* pointer = bytes) return *(ItemState*)(pointer + 32);
    }
    private static unsafe void WriteItemSave(string path, ItemState state)
    {
        Require(NativeWorldItems.Validate(&state) == 0, "valid seeded item authority fixture");
        var bytes = new byte[sizeof(ItemState) + 32];
        Marshal.Copy((IntPtr)(&state), bytes, 32, sizeof(ItemState));
        SHA256.HashData(bytes.AsSpan(32)).CopyTo(bytes, 0);
        WorldItemsProcess.AtomicWrite(path, bytes, true);
    }
}
