using Octaryn.Shared.World;

namespace Octaryn.Basegame.Content.Blocks;

public readonly record struct BlockAtlasFaceLayers(
    ushort North,
    ushort South,
    ushort East,
    ushort West,
    ushort Up,
    ushort Down)
{
    public ushort LayerFor(BlockFace face)
    {
        return face switch
        {
            BlockFace.North => North,
            BlockFace.South => South,
            BlockFace.East => East,
            BlockFace.West => West,
            BlockFace.Up => Up,
            BlockFace.Down => Down,
            _ => 0
        };
    }
}
