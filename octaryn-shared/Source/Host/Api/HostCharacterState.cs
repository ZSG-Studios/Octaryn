namespace Octaryn.Shared.Host.Api;

// Mutable kinematic character state stepped by IHostPhysicsApi.MoveCharacter.
public struct HostCharacterState
{
    public float X;
    public float Y;
    public float Z;
    public float Pitch;
    public float Yaw;
    public float VelocityX;
    public float VelocityY;
    public float VelocityZ;
    public bool IsOnGround;
    public uint ControlMode;
    public bool JumpHeld;
}
