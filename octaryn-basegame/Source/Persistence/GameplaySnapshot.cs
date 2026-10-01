using System.Buffers.Binary;
using Octaryn.Basegame.Gameplay.Inventory;
using Octaryn.Basegame.Gameplay.Items;

namespace Octaryn.Basegame.Persistence;

internal sealed record GameplaySnapshot(
    ulong ReplicationId, ulong EntityWatermark, HotbarComponent Hotbar, List<(ulong Id, WorldItemComponent State)> Items)
{
    internal const int MaximumItems = 10000;
    private const uint Magic = 0x56534742;
    private const uint Version = 1;

    internal int Write(Span<byte> destination)
    {
        var writer = new SnapshotWriter(destination);
        writer.UInt32(Magic); writer.UInt32(Version); writer.UInt64(ReplicationId);
        writer.UInt64(EntityWatermark);
        writer.UInt32((uint)Hotbar.SelectedSlot);
        foreach (var slot in Hotbar.Slots) { writer.UInt16(slot.ItemId); writer.UInt32(slot.Count); }
        writer.UInt32((uint)Items.Count);
        foreach (var (id, state) in Items)
        {
            writer.UInt64(id); writer.UInt16(state.ItemId); writer.UInt32(state.Count);
            writer.Single(state.X); writer.Single(state.Y); writer.Single(state.Z);
            writer.Single(state.VelocityX); writer.Single(state.VelocityY); writer.Single(state.VelocityZ);
            writer.Single(state.Age); writer.Single(state.SleepTimer); writer.Single(state.TraceAccumulator);
            writer.UInt32(state.Grounded ? 1u : 0u);
        }
        return writer.Written;
    }

    internal static GameplaySnapshot Read(ReadOnlySpan<byte> source, ulong playerId)
    {
        var reader = new SnapshotReader(source);
        if (reader.UInt32() != Magic || reader.UInt32() != Version)
            throw new ArgumentException("Unsupported basegame save version.");
        var replication = reader.UInt64();
        if (replication == ulong.MaxValue) throw new ArgumentException("Invalid replication identity.");
        var watermark = reader.UInt64();
        if (watermark < playerId || watermark == ulong.MaxValue) throw new ArgumentException("Invalid entity identity watermark.");
        var hotbar = new HotbarComponent { SelectedSlot = checked((int)reader.UInt32()) };
        if (hotbar.SelectedSlot >= HotbarComponent.SlotCount) throw new ArgumentException("Invalid selected slot.");
        for (var i = 0; i < HotbarComponent.SlotCount; ++i)
        {
            var item = reader.UInt16(); var count = reader.UInt32();
            ValidateStack(item, count, true);
            hotbar.Slots[i] = (item, count);
        }
        var countItems = reader.UInt32();
        if (countItems > MaximumItems) throw new ArgumentException("Too many saved world items.");
        var items = new List<(ulong Id, WorldItemComponent State)>((int)countItems);
        var ids = new HashSet<ulong>();
        for (var i = 0; i < countItems; ++i)
        {
            var id = reader.UInt64();
            if (id == 0 || id == playerId || id > watermark || !ids.Add(id))
                throw new ArgumentException("Invalid saved world-item identity.");
            var state = new WorldItemComponent
            {
                ItemId = reader.UInt16(), Count = reader.UInt32(),
                X = reader.Single(), Y = reader.Single(), Z = reader.Single(),
                VelocityX = reader.Single(), VelocityY = reader.Single(), VelocityZ = reader.Single(),
                Age = reader.Single(), SleepTimer = reader.Single(), TraceAccumulator = reader.Single()
            };
            var grounded = reader.UInt32();
            if (grounded > 1 || state.Age < 0 || state.SleepTimer < 0 || state.TraceAccumulator < 0)
                throw new ArgumentException("Invalid saved world-item state.");
            state.Grounded = grounded == 1;
            ValidateStack(state.ItemId, state.Count, false);
            items.Add((id, state));
        }
        if (!reader.Finished) throw new ArgumentException("Unexpected trailing basegame save data.");
        return new GameplaySnapshot(replication, watermark, hotbar, items);
    }

    private static void ValidateStack(ushort item, uint count, bool emptyAllowed)
    {
        if (emptyAllowed && item == 0 && count == 0) return;
        if (!ItemCatalog.TryGet(item, out var entry) || count == 0 || count > entry.MaxStack)
            throw new ArgumentException("Invalid saved item stack.");
    }
}

internal ref struct SnapshotWriter(Span<byte> destination)
{
    private readonly Span<byte> _destination = destination;
    private int _position;
    public readonly int Written => _position;
    public void UInt16(ushort value) { BinaryPrimitives.WriteUInt16LittleEndian(Take(2), value); }
    public void UInt32(uint value) { BinaryPrimitives.WriteUInt32LittleEndian(Take(4), value); }
    public void UInt64(ulong value) { BinaryPrimitives.WriteUInt64LittleEndian(Take(8), value); }
    public void Single(float value) { BinaryPrimitives.WriteSingleLittleEndian(Take(4), value); }
    private Span<byte> Take(int count)
    {
        if (count > _destination.Length - _position) throw new ArgumentException("Save destination is too small.");
        var result = _destination.Slice(_position, count); _position += count; return result;
    }
}

internal ref struct SnapshotReader(ReadOnlySpan<byte> source)
{
    private readonly ReadOnlySpan<byte> _source = source;
    private int _position;
    public readonly bool Finished => _position == _source.Length;
    public ushort UInt16() => BinaryPrimitives.ReadUInt16LittleEndian(Take(2));
    public uint UInt32() => BinaryPrimitives.ReadUInt32LittleEndian(Take(4));
    public ulong UInt64() => BinaryPrimitives.ReadUInt64LittleEndian(Take(8));
    public float Single()
    {
        var value = BinaryPrimitives.ReadSingleLittleEndian(Take(4));
        if (!float.IsFinite(value)) throw new ArgumentException("Nonfinite saved item value.");
        return value;
    }
    private ReadOnlySpan<byte> Take(int count)
    {
        if (count > _source.Length - _position) throw new ArgumentException("Truncated basegame save.");
        var result = _source.Slice(_position, count); _position += count; return result;
    }
}
