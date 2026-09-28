namespace Octaryn.Basegame.Gameplay.Actions;

// Current eye-ray interaction target, refreshed every world tick. The item
// target is the nearest world item the ray passes through, in front of any
// world surface hit; it is what "interact.use" collects and what clients
// highlight at the crosshair.
public struct LookTargetComponent
{
    public bool HasTarget;
    public uint MaterialId;
    public float Distance;
    public float PointX;
    public float PointY;
    public float PointZ;
    public bool HasItemTarget;
    public ulong ItemEntityId;
    public uint ItemId;
    public uint ItemCount;
    public float ItemDistance;
}
