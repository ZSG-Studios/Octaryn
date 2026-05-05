using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal sealed unsafe class BlockEditService(
    BlockStore blocks,
    IBlockAuthorityRules authorityRules,
    Func<BlockPosition, BlockId>? generatedBlocks = null)
{
    private const int MaxNativeChanges = 2;
    private readonly IBlockAuthorityRules _authorityRules = authorityRules;

    public BlockId GetBlock(BlockPosition position)
    {
        return blocks.TryGetBlock(position, out var block)
            ? block
            : GeneratedBlock(position);
    }

    public BlockEditResult Apply(BlockEdit edit)
    {
        var nativeEdit = NativeBlockEdit.FromBlockEdit(edit);
        var changes = new NativeBlockEdit[MaxNativeChanges];
        var handle = GCHandle.Alloc(this);
        try
        {
            fixed (NativeBlockEdit* changePointer = changes)
            {
                uint changeCount = 0;
                var result = NativeBlockStoreLibrary.BlockEditServiceApply(
                    blocks.NativeHandle,
                    &nativeEdit,
                    &GeneratedBlock,
                    &IsKnownBlock,
                    &CanApplyEdit,
                    &CanStaySupported,
                    (void*)GCHandle.ToIntPtr(handle),
                    changePointer,
                    (uint)changes.Length,
                    &changeCount);
                return ToBlockEditResult(result, changes, checked((int)changeCount));
            }
        }
        finally
        {
            handle.Free();
        }
    }

    public BlockEditResult ApplyCommand(HostCommand command)
    {
        var changes = new NativeBlockEdit[MaxNativeChanges];
        var handle = GCHandle.Alloc(this);
        try
        {
            fixed (NativeBlockEdit* changePointer = changes)
            {
                uint changeCount = 0;
                var result = NativeBlockStoreLibrary.BlockEditServiceApplyCommand(
                    blocks.NativeHandle,
                    &command,
                    &GeneratedBlock,
                    &IsKnownBlock,
                    &CanApplyEdit,
                    &CanStaySupported,
                    (void*)GCHandle.ToIntPtr(handle),
                    changePointer,
                    (uint)changes.Length,
                    &changeCount);
                return ToBlockEditResult(result, changes, checked((int)changeCount));
            }
        }
        finally
        {
            handle.Free();
        }
    }

    internal bool CanApply(BlockEdit edit)
    {
        var nativeEdit = NativeBlockEdit.FromBlockEdit(edit);
        var handle = GCHandle.Alloc(this);
        try
        {
            return NativeBlockStoreLibrary.BlockEditServiceCanApply(
                blocks.NativeHandle,
                &nativeEdit,
                &GeneratedBlock,
                &IsKnownBlock,
                &CanApplyEdit,
                (void*)GCHandle.ToIntPtr(handle)) != 0;
        }
        finally
        {
            handle.Free();
        }
    }

    internal bool CanApplyCommand(HostCommand command)
    {
        var handle = GCHandle.Alloc(this);
        try
        {
            return NativeBlockStoreLibrary.BlockEditServiceCanApplyCommand(
                blocks.NativeHandle,
                &command,
                &GeneratedBlock,
                &IsKnownBlock,
                &CanApplyEdit,
                (void*)GCHandle.ToIntPtr(handle)) != 0;
        }
        finally
        {
            handle.Free();
        }
    }

    private BlockId GeneratedBlock(BlockPosition position)
    {
        return generatedBlocks?.Invoke(position) ?? BlockId.Air;
    }

    private static BlockEditResult ToBlockEditResult(NativeBlockEditResult result, NativeBlockEdit[] changes, int changeCount)
    {
        if (result.Changed == 0)
        {
            return result.Applied != 0 ? BlockEditResult.Unchanged : default;
        }

        if (changeCount <= 0)
        {
            return BlockEditResult.ChangedEdit(result.Edit.ToBlockEdit());
        }

        var boundedChangeCount = Math.Min(changeCount, changes.Length);
        var managedChanges = new BlockEdit[boundedChangeCount];
        for (var index = 0; index < boundedChangeCount; index++)
        {
            managedChanges[index] = changes[index].ToBlockEdit();
        }

        return new BlockEditResult(Applied: true, Changed: true, Changes: managedChanges);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static ushort GeneratedBlock(void* context, NativeBlockPosition* position)
    {
        return TryGetService(context, out var service) && position is not null
            ? service.GeneratedBlock(position->ToBlockPosition()).Value
            : BlockId.Air.Value;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint IsKnownBlock(void* context, ushort block)
    {
        return TryGetService(context, out var service) &&
            service._authorityRules.IsKnownBlock(new BlockId(block))
                ? 1u
                : 0u;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint CanApplyEdit(void* context, NativeBlockEdit* edit, ushort belowBlock)
    {
        return TryGetService(context, out var service) &&
            edit is not null &&
            service._authorityRules.CanApplyEdit(edit->ToBlockEdit(), new BlockId(belowBlock))
                ? 1u
                : 0u;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint CanStaySupported(
        void* context,
        ushort block,
        NativeBlockPosition* position,
        ushort belowBlock)
    {
        return TryGetService(context, out var service) &&
            position is not null &&
            service._authorityRules.CanStaySupported(
                new BlockId(block),
                position->ToBlockPosition(),
                new BlockId(belowBlock))
                ? 1u
                : 0u;
    }

    private static bool TryGetService(void* context, out BlockEditService service)
    {
        service = null!;
        if (context is null)
        {
            return false;
        }

        var handle = GCHandle.FromIntPtr((IntPtr)context);
        if (handle.Target is not BlockEditService target)
        {
            return false;
        }

        service = target;
        return true;
    }
}
