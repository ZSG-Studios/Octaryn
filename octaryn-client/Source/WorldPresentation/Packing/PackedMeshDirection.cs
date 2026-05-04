using Octaryn.Shared.World;

namespace Octaryn.Client.WorldPresentation;

internal enum PackedMeshDirection : byte
{
    North = 0,
    South = 1,
    East = 2,
    West = 3,
    Up = 4,
    Down = 5
}

internal static class PackedMeshDirectionMap
{
    public static PackedMeshDirection FromDirection(Direction direction)
    {
        return direction switch
        {
            Direction.PositiveZ => PackedMeshDirection.North,
            Direction.NegativeZ => PackedMeshDirection.South,
            Direction.PositiveX => PackedMeshDirection.East,
            Direction.NegativeX => PackedMeshDirection.West,
            Direction.PositiveY => PackedMeshDirection.Up,
            Direction.NegativeY => PackedMeshDirection.Down,
            _ => throw new ArgumentOutOfRangeException(nameof(direction), direction, "Unsupported mesh direction")
        };
    }

    public static int ToAtlasDirectionIndex(Direction direction)
    {
        return (int)FromDirection(direction);
    }
}
