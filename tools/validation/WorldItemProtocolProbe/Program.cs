using System.Runtime.InteropServices;
using Octaryn.Client.Host.Remote;
using Octaryn.Server.Networking.Remote;
using Octaryn.Shared.Host.Api;
using Octaryn.Shared.Networking.Remote;

void Check(bool value, string message) { if (!value) throw new Exception(message); }
HostWorldItemPose Pose(ulong id, float x = 0) => new() { EntityId = id, ItemId = 2, Count = 1, X = x, Y = 1 };
Check(Marshal.SizeOf<HostWorldItemPose>() == 64 && Marshal.SizeOf<WorldItemVisualPose>() == 88, "ABI changed");
var registry = new WorldItemRegistry();
for (ulong id = 1; id <= 10000; ++id) Check(registry.Publish(Pose(id), 1), "Initial item admission");
Check(registry.Available == 0 && !registry.Publish(Pose(10001), 1), "Registry exceeded bound");
Check(registry.Publish(Pose(1, 4), 2), "Full registry refused existing motion");
var baseline = registry.BeginBaseline();
Check(baseline.Length == 10000 && baseline[0].X == 4, "Baseline did not coalesce motion");
var client = new WorldItemSnapshots();
ulong sequence = 0;
for (int offset = 0; offset < baseline.Length; offset += 16)
{
    var size = Math.Min(16, baseline.Length - offset);
    byte flags = offset == 0 ? WorldItemPacket.BeginBaseline : (byte)0;
    if (offset + size == baseline.Length) flags |= WorldItemPacket.EndBaseline | WorldItemPacket.EndBatch;
    var packet = WorldItemPacket.Encode(1, ++sequence, flags, baseline.AsSpan(offset, size));
    Check(client.Accept(packet, out _), "Baseline decode failed");
    if (offset + size != baseline.Length) Check(client.Published.Poses.Length == 0, "Partial baseline became visible");
}
Check(client.Published.Poses.Length == 10000, "Complete baseline was not published");
var before = client.Published;
var removed = Pose(1); removed.Flags = HostWorldItemPose.Removed;
Check(registry.Publish(removed, 3), "Removal failed");
Check(registry.Available == 0, "Undelivered removal did not retain capacity");
Span<HostWorldItemPose> delta = stackalloc HostWorldItemPose[16];
Check(registry.Drain(delta) == 1 && registry.Available == 1, "Tombstone did not drain");
var deletion = WorldItemPacket.Encode(1, ++sequence, WorldItemPacket.EndBatch, delta[..1]);
Check(client.Accept(deletion, out var ack) && ack is { Length: 20 }, "Removal ACK missing");
Check(client.Published.Poses.Length == 9999 && before.Poses.Length == 10000, "Snapshot mutability/removal failure");
Check(client.Accept(deletion, out _) && client.Published.Poses.Length == 9999, "Duplicate removal replayed");
Check(registry.Publish(Pose(1, 9), 4), "Reused entity did not get new generation");
Check(registry.Drain(delta) == 1 && delta[0].Generation > baseline[0].Generation, "Generation was reused");
Check(client.Accept(WorldItemPacket.Encode(1, ++sequence, 4, delta[..1]), out _), "New generation decode");
var reborn = client.Published.Poses.Single(p => p.Current.EntityId == 1);
Check(reborn.PreviousX == 9 && reborn.PreviousTick == 4, "New generation inherited old interpolation history");
Check(registry.Publish(Pose(1, 10), 5) && registry.Drain(delta) == 1, "Motion delta");
Check(client.Accept(WorldItemPacket.Encode(1, ++sequence, 4, delta[..1]), out _), "Motion decode");
var moved = client.Published.Poses.Single(p => p.Current.EntityId == 1);
Check(moved.PreviousX == 9 && moved.Current.X == 10 && moved.PreviousTick == 4, "Motion history mismatch");
var bad = delta[0]; bad.X = float.NaN;
Check(!client.Accept(WorldItemPacket.Encode(1, sequence + 1, 4, new[] { bad }), out _), "Nonfinite pose accepted");
Check(!client.Accept(WorldItemPacket.Encode(1, sequence + 2, 4, delta[..1]), out _), "Sequence gap accepted");
Check(client.Accept(WorldItemPacket.Encode(2, 1, 7, ReadOnlySpan<HostWorldItemPose>.Empty), out _), "Reconnect empty baseline failed");
Check(client.Published.Poses.Length == 0 && !client.Accept(deletion, out _), "Stale epoch resurrected items");
Console.WriteLine("world_item_protocol=passed bound=10000 atomic_baseline=1 coalescing=1 removal=1 generations=1 reconnect=1 immutable_snapshot=1 validation=1");
