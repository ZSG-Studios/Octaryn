using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Generation;

internal sealed class NativeEmptyWorldGenerator
{
    public static BlockId WhiteBlock => new(1);

    public BlockId GetGeneratedBlock(BlockPosition position)
    {
        return IsGeneratedSolidBlock(position) ? WhiteBlock : BlockId.Air;
    }

    public bool IsGeneratedSolidBlock(BlockPosition position)
    {
        return position.Y >= BlockLimits.WorldMinY && position.Y < 0;
    }
}
