namespace Octaryn.Shared.Host.Api;

public interface IHostPhysicsApi
{
    // Direction need not be normalized. Returns false on miss or error.
    bool Raycast(
        float originX, float originY, float originZ,
        float directionX, float directionY, float directionZ,
        float maxDistance, out HostRaycastHit hit);

    // Steps a module-owned kinematic character against the collision world.
    // False when the host has no collision world or the step failed.
    bool MoveCharacter(in HostCharacterInput input, double deltaSeconds, ref HostCharacterState state);

    // Steps one dropped item body against the collision world: swept motion
    // with restitution bounces, contact friction, and settle-to-sleep. False
    // when the host has no collision world; the module decides how to degrade.
    bool StepWorldItem(ref HostWorldItemState state, double deltaSeconds);
}
