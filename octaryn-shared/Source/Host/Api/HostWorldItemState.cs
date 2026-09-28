namespace Octaryn.Shared.Host.Api;

// Ballistic state of one dropped item for IHostPhysicsApi.StepWorldItem.
// Position is the collision capsule centre in world space. Sleeping items
// are settled against the world and skip simulation until the module moves
// them or applies a new impulse.
public struct HostWorldItemState
{
    public float X;
    public float Y;
    public float Z;
    public float VelocityX;
    public float VelocityY;
    public float VelocityZ;
    public bool Grounded;
    public bool Sleeping;
    // Physics-owned settle timer; modules must round-trip it unchanged.
    public float SleepTimer;
}
