using Octaryn.Shared.World;

namespace Octaryn.Shared.GameModules;

public enum ModuleCommandRequestKind : uint
{
    None = 0,
    SetBlock = 1
}

public readonly record struct ModuleCommandRequest(
    ModuleCommandRequestKind Kind,
    ulong RequestId,
    BlockEdit BlockEdit)
{
    public static ModuleCommandRequest SetBlock(BlockEdit edit, ulong requestId = 0)
    {
        return new ModuleCommandRequest(ModuleCommandRequestKind.SetBlock, requestId, edit);
    }

    public static ModuleCommandRequest BreakBlock(BlockPosition position, ulong requestId = 0)
    {
        return SetBlock(new BlockEdit(position, BlockId.Air), requestId);
    }
}
