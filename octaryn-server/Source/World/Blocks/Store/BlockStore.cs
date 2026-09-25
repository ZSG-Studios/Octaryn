using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed unsafe class BlockStore : IDisposable
{
    private IntPtr _handle;

    public BlockStore()
    {
        _handle = NativeBlockStoreLibrary.BlockStoreCreate();
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Native server block store allocation failed.");
        }
    }

    ~BlockStore()
    {
        Dispose();
    }

    public int BlockCount => checked((int)NativeBlockStoreLibrary.BlockStoreBlockCount(Handle));

    internal IntPtr NativeHandle => Handle;

    public BlockId GetBlock(BlockPosition position)
    {
        var nativePosition = NativeBlockPosition.FromBlockPosition(position);
        return new BlockId(NativeBlockStoreLibrary.BlockStoreGetBlock(Handle, &nativePosition));
    }

    public BlockEditResult SetBlock(BlockEdit edit, bool preserveAirOverride = false)
    {
        var nativeEdit = NativeBlockEdit.FromBlockEdit(edit);
        return NativeBlockStoreLibrary.BlockStoreSetBlock(Handle, &nativeEdit, preserveAirOverride ? 1u : 0u).ToBlockEditResult();
    }

    public IReadOnlyList<BlockEdit> Snapshot()
    {
        var count = checked((int)NativeBlockStoreLibrary.BlockStoreSnapshotCount(Handle));
        if (count == 0)
        {
            return [];
        }

        var nativeEdits = new NativeBlockEdit[count];
        fixed (NativeBlockEdit* editPointer = nativeEdits)
        {
            var written = checked((int)NativeBlockStoreLibrary.BlockStoreSnapshotFill(Handle, editPointer, (ulong)nativeEdits.Length));
            return ToBlockEdits(nativeEdits, written);
        }
    }

    public void Load(IEnumerable<BlockEdit> edits)
    {
        var nativeEdits = edits.Select(NativeBlockEdit.FromBlockEdit).ToArray();
        fixed (NativeBlockEdit* editPointer = nativeEdits)
        {
            NativeBlockStoreLibrary.BlockStoreLoad(Handle, editPointer, (ulong)nativeEdits.Length);
        }
    }

    public void Dispose()
    {
        var handle = _handle;
        if (handle == IntPtr.Zero)
        {
            return;
        }

        _handle = IntPtr.Zero;
        NativeBlockStoreLibrary.BlockStoreDestroy(handle);
        GC.SuppressFinalize(this);
    }

    private IntPtr Handle
    {
        get
        {
            ObjectDisposedException.ThrowIf(_handle == IntPtr.Zero, this);
            return _handle;
        }
    }

    private static BlockEdit[] ToBlockEdits(NativeBlockEdit[] nativeEdits, int count)
    {
        var edits = new BlockEdit[count];
        for (var index = 0; index < count; index++)
        {
            edits[index] = nativeEdits[index].ToBlockEdit();
        }

        return edits;
    }
}
