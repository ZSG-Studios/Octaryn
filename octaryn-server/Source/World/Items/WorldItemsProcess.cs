using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using Octaryn.Server.Modules;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Items;

// A single standalone local-player process owns this transport and its durable receipts.
internal sealed unsafe class WorldItemsProcess : IDisposable
{
    private readonly ModuleActivator _module;
    private readonly string _save, _intent, _snapshot;
    private readonly Stopwatch _clock = Stopwatch.StartNew();
    private ItemState _state;
    private double _lastTick, _lastSave, _lastPublish;
    private bool _needsSave, _needsPublish, _disposed;
    private GCHandle _self;
    private bool _callbackFailed;

    internal WorldItemsProcess(ModuleActivator module, string? worldRoot = null, string? runtimeRoot = null)
    {
        _module = module;
        worldRoot ??= NativeWorldPersistenceLibrary.WorldRootPathFromEnvironment();
        runtimeRoot ??= Path.Combine(worldRoot, "runtime");
        Directory.CreateDirectory(worldRoot);
        Directory.CreateDirectory(runtimeRoot);
        _save = Path.Combine(worldRoot, "world_items.bin");
        _intent = Path.Combine(runtimeRoot, "world_items.intent");
        _snapshot = Path.Combine(runtimeRoot, "world_items.snapshot");
        fixed (ItemState* state = &_state) NativeWorldItems.Initialize(state);
        if (File.Exists(_save))
        {
            var bytes = Read(_save, sizeof(ItemState) + 32);
            if (bytes.Length != sizeof(ItemState) + 32 ||
                !CryptographicOperations.FixedTimeEquals(SHA256.HashData(bytes.AsSpan(32)), bytes.AsSpan(0, 32)))
                throw new InvalidDataException("World item persistence checksum is invalid.");
            fixed (byte* source = bytes) _state = *(ItemState*)(source + 32);
            fixed (ItemState* state = &_state)
                if (NativeWorldItems.Validate(state) != 0) throw new InvalidDataException("World item persistence is invalid.");
        }
        else { _needsSave = true; Save(); }
        _self = GCHandle.Alloc(this);
        try { Publish(); }
        catch { _self.Free();throw; }
    }

    internal void RequestSnapshot() => _needsPublish = true;

    internal void Step(double? sourceSeconds = null)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        var now = sourceSeconds ?? _clock.Elapsed.TotalSeconds;
        if (!double.IsFinite(now) || now < _lastTick) throw new ArgumentOutOfRangeException(nameof(sourceSeconds));
        // A failed durable transaction is retried before any further mutation/publication.
        if (_needsSave) Save();
        var player = _module.SnapshotPlayer();
        var beforeCommand = _state.LastCommand;
        var beforeGrant = _state.NextGrant;
        var beforeAck = _state.AcknowledgedGrant;
        var beforeCount = _state.ItemCount;
        ApplyIntent(player.X, player.Y, player.Z, player.Yaw, player.Pitch);
        var delta = Math.Clamp(now - _lastTick, 0, .25);
        _lastTick = now;
        _callbackFailed = false;
        fixed (ItemState* state = &_state)
            if (NativeWorldItems.Tick(state, delta, player.X, player.Y, player.Z, &Solid,
                (void*)GCHandle.ToIntPtr(_self)) != 0 || _callbackFailed)
                throw new InvalidOperationException("Authoritative world item physics failed.");
        var transaction = beforeCommand != _state.LastCommand || beforeGrant != _state.NextGrant ||
            beforeAck != _state.AcknowledgedGrant || beforeCount != _state.ItemCount;
        if (transaction || now - _lastPublish >= 1.0 / 30 && _state.ItemCount != 0) _needsPublish = true;
        if (transaction || now - _lastSave >= 1 && _state.ItemCount != 0)
        {
            _needsSave = true;
            Save();
        }
        if (_needsPublish) Publish();
    }

    private void ApplyIntent(float x, float y, float z, float yaw, float pitch)
    {
        byte[] bytes;
        try { bytes = Read(_intent, 32); }
        catch (FileNotFoundException) { return; }
        catch (IOException) { return; } // Atomic publisher contention retains the previous complete intent.
        if (bytes.Length != 32) return;
        using var reader = new BinaryReader(new MemoryStream(bytes));
        if (reader.ReadUInt32() != 1 || reader.ReadUInt32() != 32) return;
        var command = reader.ReadUInt64();
        var acknowledge = reader.ReadUInt64();
        var block = reader.ReadUInt32();
        var count = reader.ReadUInt32();
        fixed (ItemState* state = &_state)
        {
            // Acknowledgements may only retire the oldest outstanding local-player grant.
            NativeWorldItems.Acknowledge(state, acknowledge);
            if (command > _state.LastCommand)
                NativeWorldItems.Drop(state, command, block, count,
                    block <= ushort.MaxValue && _module.IsItemBlockPlaceable((ushort)block) ? 1u : 0u,
                    x, y, z, yaw, pitch);
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint Solid(void* context, int x, int y, int z)
    {
        var owner = (WorldItemsProcess)GCHandle.FromIntPtr((IntPtr)context).Target!;
        try { return owner._module.IsItemCollisionSolid(new BlockPosition(x, y, z)) ? 1u : 0u; }
        catch { owner._callbackFailed = true; return 1; }
    }

    private byte[] SnapshotBytes()
    {
        var bytes = new byte[sizeof(ItemState)];
        fixed (ItemState* state = &_state) Marshal.Copy((IntPtr)state, bytes, 0, bytes.Length);
        return bytes;
    }
    private void Save()
    {
        var bytes = SnapshotBytes();
        var persisted = new byte[32 + bytes.Length];
        SHA256.HashData(bytes).CopyTo(persisted, 0);bytes.CopyTo(persisted, 32);
        AtomicWrite(_save, persisted, durable: true);
        _needsSave = false;_lastSave = _lastTick;
    }
    private void Publish()
    {
        AtomicWrite(_snapshot, SnapshotBytes(), durable: false);
        _needsPublish = false;
        _lastPublish = _lastTick;
    }
    internal static byte[] Read(string path, int maximum)
    {
        using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        if (file.Length > maximum) throw new InvalidDataException("World item file exceeds its bounded schema.");
        var bytes = new byte[checked((int)file.Length)];file.ReadExactly(bytes);return bytes;
    }
    internal static bool IsTransientFileContention(Exception error) =>
        OperatingSystem.IsWindows() && error is IOException or UnauthorizedAccessException &&
        (error.HResult & 0xffff) is 5 or 32 or 33;

    internal static void AtomicWrite(string path, byte[] bytes, bool durable)
    {
        var temporary = path + ".tmp";
        using (var file = new FileStream(temporary, FileMode.Create, FileAccess.Write, FileShare.Read))
        { file.Write(bytes);file.Flush(durable); }
        File.Move(temporary, path, overwrite: true);
    }
    public void Dispose()
    {
        if (_disposed) return;
        try { Save(); }
        finally { if (_self.IsAllocated) _self.Free();_disposed = true; }
    }
}
