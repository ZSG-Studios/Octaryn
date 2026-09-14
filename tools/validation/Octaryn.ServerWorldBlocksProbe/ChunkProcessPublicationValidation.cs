using System.Text.Json;
using Octaryn.Server;
using Octaryn.Server.Modules;
using Octaryn.Server.World.Chunks;

internal static partial class ServerWorldBlocksProbe
{
    private static void ValidateProcessPublication()
    {
        var root = ResetProbeDirectory("process-publication");
        var previousPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_BLOCKS_PATH");
        SetPersistencePath(Path.Combine(root, "world_blocks.json"));
        try
        {
            using var activator = new ModuleActivator(new BlockEditRegistration());
            Require(activator.Activate(new RejectingCommandSink()) == 0, "publication bridge fixture activation");
            var window = new NativeChunkViewIntent(1, 51, 0, 0, 1, 0, 0, 0, 0);
            var path = Path.Combine(root, "first.json");
            int Publish(string destination) => ChunkStreamProcessBridge.PublishSnapshot(
                activator, destination, window, metadataOnly: true, submittedBlockCommands: false, liveProcess: true);
            Require(Publish(path) == 0, "actual bridge initial publication");
            Require(File.Exists(path + ".bin") && ReadPublicationBlocks(path).Length == 0,
                "actual initial native JSON and binary snapshot exist");
            var initialBinary = File.ReadAllBytes(path + ".bin");
            activator.Tick(Frame(1)); // Module-originated edit, no client command or new window.
            Require(activator.BlockRevision == 1, "module tick produces autonomous revision");
            var timeBeforeWrite = activator.SnapshotWorldTime().TotalWorldSeconds;
            Require(Publish(path) == 0, "same-window autonomous revision bypasses live skip");
            var changes = ReadPublicationBlocks(path);
            Require(changes.Length == 1 && changes[0] == (8, 9, 10, 5),
                "metadata request overridden by full authoritative module edit snapshot");
            Require(!initialBinary.SequenceEqual(File.ReadAllBytes(path + ".bin")),
                "autonomous edit updates actual binary client stream");
            Require(activator.SnapshotWorldTime().TotalWorldSeconds == timeBeforeWrite,
                "publication must not execute an additional simulation tick");

            var second = Path.Combine(root, "second.json");
            var blockedBinary = second + ".bin";
            Directory.CreateDirectory(blockedBinary);
            var marker = Path.Combine(blockedBinary, "preserve.txt");
            File.WriteAllText(marker, "nonempty fixture directory prevents native replacement");
            var failed = false;
            try { Publish(second); }
            catch (InvalidOperationException) { failed = true; }
            Require(failed && activator.ChunkPublication.NeedsFullSnapshot(second, activator.BlockRevision),
                "actual native binary write failure must not acknowledge revision");
            File.Delete(marker);
            Directory.Delete(blockedBinary);
            Require(Publish(second) == 0 && ReadPublicationBlocks(second).Length == 1,
                "actual native retry publishes all overrides without a new command");
            Require(activator.ChunkPublication.NeedsFullSnapshot(path, activator.BlockRevision),
                "actual A to B to A destination requires fresh full baseline");
            Require(Publish(path) == 0 && ReadPublicationBlocks(path).Length == 1,
                "return destination receives full existing override");

            window = new NativeChunkViewIntent(1, 52, 0, 0, 1, 1, 0, 0, 1);
            Require(Publish(path) == 0, "epoch-only metadata request must bypass native unchanged-window skip");
            using (var snapshot = JsonDocument.Parse(File.ReadAllText(path)))
                Require(snapshot.RootElement.GetProperty("epoch").GetUInt64() == 52,
                    "actual JSON header must publish new epoch");
            var binary = ReadPublicationBinary(path + ".bin");
            Require(binary.Epoch == 52 && binary.Blocks.SequenceEqual(ReadPublicationBlocks(path)) &&
                binary.Blocks.Length == 1 && binary.Blocks[0] == (8, 9, 10, 5),
                "epoch-only native binary must retain exact authoritative overrides and new epoch");
            File.SetLastWriteTimeUtc(path + ".bin", new DateTime(2001, 1, 1, 0, 0, 0, DateTimeKind.Utc));
            var idleTimestamp = File.GetLastWriteTimeUtc(path + ".bin");
            Require(Publish(path) == 0 && File.GetLastWriteTimeUtc(path + ".bin") == idleTimestamp,
                "acknowledged epoch-only request must skip subsequent unchanged native write");

            window = new NativeChunkViewIntent(1, 53, 1, 0, 1, 1, 0, 0, 1);
            Require(Publish(path) == 0, "moving window with previous overlap must publish");
            using (var snapshot = JsonDocument.Parse(File.ReadAllText(path)))
                Require(snapshot.RootElement.GetProperty("centerChunkX").GetInt32() == 1,
                    "actual JSON header must publish moved center");
            binary = ReadPublicationBinary(path + ".bin");
            Require(binary.Epoch == 53 && binary.Blocks.SequenceEqual(ReadPublicationBlocks(path)) &&
                binary.Blocks.Length == 1 && binary.Blocks[0] == (8, 9, 10, 5),
                "moving window must retain edited preserved column in JSON and binary");

            // An unchanged revision/window must not touch even a now-unwritable destination.
            File.Delete(path + ".bin");
            Directory.CreateDirectory(path + ".bin");
            File.WriteAllText(Path.Combine(path + ".bin", "preserve.txt"), "idle write guard");
            Require(Publish(path) == 0 && Directory.Exists(path + ".bin"),
                "unchanged bridge call must skip native output rather than rewrite");
            Console.WriteLine("chunk_process_publication=passed native_json_binary=passed autonomous=passed native_retry=passed epoch_only=passed retained_overrides=passed moved_window=passed");
        }
        finally { RestorePersistencePath(previousPath); }
    }

    private static (int X, int Y, int Z, int Block)[] ReadPublicationBlocks(string path)
    {
        using var snapshot = JsonDocument.Parse(File.ReadAllText(path));
        return snapshot.RootElement.GetProperty("blocks").EnumerateArray()
            .Select(block => (block.GetProperty("x").GetInt32(), block.GetProperty("y").GetInt32(),
                block.GetProperty("z").GetInt32(), block.GetProperty("block").GetInt32())).ToArray();
    }

    private static (ulong Epoch, (int X, int Y, int Z, int Block)[] Blocks) ReadPublicationBinary(string path)
    {
        using var input = new BinaryReader(File.OpenRead(path));
        Require(System.Text.Encoding.ASCII.GetString(input.ReadBytes(8)) == "OCSTRM01" && input.ReadUInt32() == 2,
            "native publication binary version");
        var epoch = input.ReadUInt64();
        input.BaseStream.Position = 112;
        var columns = input.ReadUInt32();
        var count = input.ReadUInt32();
        Require(columns == 9 && count == 1 && input.BaseStream.Length == 120L + 24L * columns + 14L * count,
            "native epoch fixture binary record bounds");
        input.BaseStream.Position = 120L + 24L * columns;
        var blocks = new (int X, int Y, int Z, int Block)[checked((int)count)];
        for (var index = 0; index < blocks.Length; ++index)
            blocks[index] = (input.ReadInt32(), input.ReadInt32(), input.ReadInt32(), input.ReadUInt16());
        return (epoch, blocks);
    }
}
