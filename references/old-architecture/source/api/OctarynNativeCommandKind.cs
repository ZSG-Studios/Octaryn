namespace Octaryn.Engine.Api;

public enum OctarynNativeCommandKind : uint
{
    None = 0,
    SetBlock = 1,
    SpawnPhysicsBody = 2,
    DestroyPhysicsBody = 3,
    SetBodyVelocity = 4,
    ApplyBodyImpulse = 5,
    Raycast = 6,
    NetworkEvent = 7
}
