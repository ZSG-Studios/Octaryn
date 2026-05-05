using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
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

    public bool TryGetBlock(BlockPosition position, out BlockId block)
    {
        var nativePosition = NativeBlockPosition.FromBlockPosition(position);
        ushort nativeBlock = 0;
        var found = NativeBlockStoreLibrary.BlockStoreTryGetBlock(Handle, &nativePosition, &nativeBlock) != 0;
        block = new BlockId(nativeBlock);
        return found;
    }

    public BlockEditResult ClearBlockOverride(BlockPosition position)
    {
        var nativePosition = NativeBlockPosition.FromBlockPosition(position);
        return NativeBlockStoreLibrary.BlockStoreClearBlockOverride(Handle, &nativePosition).ToBlockEditResult();
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

    public IReadOnlyList<BlockEdit> SnapshotChunkColumn(int originX, int originZ)
    {
        var count = checked((int)NativeBlockStoreLibrary.BlockStoreSnapshotChunkColumnCount(Handle, originX, originZ));
        if (count == 0)
        {
            return [];
        }

        var nativeEdits = new NativeBlockEdit[count];
        fixed (NativeBlockEdit* editPointer = nativeEdits)
        {
            var written = checked((int)NativeBlockStoreLibrary.BlockStoreSnapshotChunkColumnFill(
                Handle,
                originX,
                originZ,
                editPointer,
                (ulong)nativeEdits.Length));
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

    public int ClearOverridesMatching(Func<BlockPosition, BlockId> generatedBlocks)
    {
        var handle = GCHandle.Alloc(generatedBlocks);
        try
        {
            return NativeBlockStoreLibrary.BlockStoreClearOverridesMatching(
                Handle,
                &GetGeneratedBlock,
                (void*)GCHandle.ToIntPtr(handle));
        }
        finally
        {
            handle.Free();
        }
    }

    public static bool IsValidPosition(BlockPosition position)
    {
        var nativePosition = NativeBlockPosition.FromBlockPosition(position);
        return NativeBlockStoreLibrary.BlockStoreIsValidPosition(&nativePosition) != 0;
    }

    public static ChunkPosition ChunkPositionFor(BlockPosition position)
    {
        var nativePosition = NativeBlockPosition.FromBlockPosition(position);
        return NativeBlockStoreLibrary.BlockStoreChunkPositionFor(&nativePosition).ToChunkPosition();
    }

    public static BlockPosition LocalPositionFor(BlockPosition position)
    {
        var nativePosition = NativeBlockPosition.FromBlockPosition(position);
        return NativeBlockStoreLibrary.BlockStoreLocalPositionFor(&nativePosition).ToBlockPosition();
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

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static ushort GetGeneratedBlock(void* context, NativeBlockPosition* position)
    {
        if (context is null || position is null)
        {
            return BlockId.Air.Value;
        }

        var handle = GCHandle.FromIntPtr((IntPtr)context);
        return handle.Target is Func<BlockPosition, BlockId> generatedBlocks
            ? generatedBlocks(position->ToBlockPosition()).Value
            : BlockId.Air.Value;
    }
}
