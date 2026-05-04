using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Generation;

internal sealed class ServerNativeEmptyWorldGenerator
{
    public static BlockId WhiteBlock => new(1);

    public BlockId GetGeneratedBlock(BlockPosition position)
    {
        return IsGeneratedSolidBlock(position) ? WhiteBlock : BlockId.Air;
    }

    public bool IsGeneratedSolidBlock(BlockPosition position)
    {
        return position.Y >= ServerBlockLimits.WorldMinY && position.Y < 0;
    }
}
