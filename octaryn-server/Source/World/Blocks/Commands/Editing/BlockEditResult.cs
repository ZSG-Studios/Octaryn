using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal readonly record struct BlockEditResult(bool Applied, bool Changed, IReadOnlyList<BlockEdit> Changes)
{
    public static BlockEditResult Unchanged => new(Applied: true, Changed: false, Changes: []);

    public static BlockEditResult ChangedEdit(BlockEdit edit)
    {
        return new BlockEditResult(Applied: true, Changed: true, Changes: [edit]);
    }
}
