using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed unsafe class FluidSimulation : IDisposable
{
    private IntPtr _handle;
    private bool _configured;
    private int _centerX, _centerZ;
    private uint _radius;

    public FluidSimulation(FluidRules rules, IBlockAuthorityRules authority)
    {
        ArgumentNullException.ThrowIfNull(rules);
        ArgumentNullException.ThrowIfNull(authority);
        foreach (var block in rules.WaterLevels.Concat(rules.LavaLevels))
            if (!authority.IsKnownBlock(block) || authority.IsSolidBlock(block))
                throw new InvalidOperationException("Fluid levels must resolve to known non-solid authority blocks.");
        foreach (var block in rules.Replaceable)
            if (!authority.IsKnownBlock(block))
                throw new InvalidOperationException("Fluid replaceable block is not in the authority catalog.");
        foreach (var block in rules.Solid)
            if (!authority.IsKnownBlock(block) || !authority.IsSolidBlock(block))
                throw new InvalidOperationException("Fluid solid block does not match the authority catalog.");
        var replaceable = rules.Replaceable.Select(block => block.Value).ToArray();
        var solid = rules.Solid.Select(block => block.Value).ToArray();
        fixed (ushort* replaceablePointer = replaceable, solidPointer = solid)
        {
            var config = new NativeFluidConfig {
                Version = 1, Size = (uint)sizeof(NativeFluidConfig), Stone = rules.Stone.Value,
                ReplaceableCount = (uint)replaceable.Length, SolidCount = (uint)solid.Length,
                Replaceable = replaceablePointer, Solid = solidPointer
            };
            for (var index = 0; index < 8; ++index)
            {
                config.Water[index] = rules.WaterLevels[index].Value;
                config.Lava[index] = rules.LavaLevels[index].Value;
            }
            _handle = NativeFluidLibrary.Create(&config);
        }
        if (_handle == IntPtr.Zero) throw new InvalidOperationException("Native fluid configuration/allocation failed.");
    }

    ~FluidSimulation() { Dispose(); }

    public void SetRegion(int centerX, int centerZ, uint radius)
    {
        if (_configured && centerX == _centerX && centerZ == _centerZ && radius == _radius) return;
        if (NativeFluidLibrary.SetRegion(Handle, centerX, centerZ, radius) != 0)
            throw new InvalidOperationException("Native fluid region rejected.");
        _centerX = centerX; _centerZ = centerZ; _radius = radius; _configured = true;
    }

    public void Wake(IReadOnlyList<BlockEdit> changes)
    {
        if (!_configured || changes.Count == 0) return;
        const int batchCapacity = 64;
        NativeBlockEdit* buffer = stackalloc NativeBlockEdit[batchCapacity];
        for (var offset = 0; offset < changes.Count; offset += batchCapacity)
        {
            var count = Math.Min(batchCapacity, changes.Count - offset);
            for (var index = 0; index < count; ++index)
                buffer[index] = NativeBlockEdit.FromBlockEdit(changes[offset + index]);
            if (NativeFluidLibrary.Notify(Handle, buffer, (uint)count) != 0)
                throw new InvalidOperationException("Native fluid change notification failed.");
        }
    }

    public NativeFluidTickReport Advance(double deltaSeconds, BlockEditService edits, BlockChangeQueue? queue)
    {
        if (!_configured || !double.IsFinite(deltaSeconds) || deltaSeconds < 0) return default;
        return edits.TickFluids(Handle, queue, Math.Min(deltaSeconds, 0.25));
    }

    public void Dispose()
    {
        var handle = _handle;
        if (handle == IntPtr.Zero) return;
        _handle = IntPtr.Zero;
        NativeFluidLibrary.Destroy(handle);
        GC.SuppressFinalize(this);
    }

    private IntPtr Handle
    {
        get { ObjectDisposedException.ThrowIf(_handle == IntPtr.Zero, this); return _handle; }
    }
}
