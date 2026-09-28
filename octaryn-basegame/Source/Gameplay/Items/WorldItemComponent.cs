namespace Octaryn.Basegame.Gameplay.Items;

// A stack of items resting in or moving through the world: thrown drops,
// pickups awaiting collection. Position and velocity are authoritative.
public struct WorldItemComponent
{
    public ushort ItemId;
    public uint Count;
    public float X;
    public float Y;
    public float Z;
    public float VelocityX;
    public float VelocityY;
    public float VelocityZ;
    public bool Grounded;
    // Seconds since the drop; tossed items only become collectible once they
    // settle (or, on hosts without a collision world, once the toss is over),
    // so a throw never vacuums straight back into the thrower's hotbar.
    public float Age;
    // Settle timer owned by the physics step; round-tripped unchanged.
    public float SleepTimer;
    // Diagnostic pacing for the airborne trace.
    public float TraceAccumulator;
}
