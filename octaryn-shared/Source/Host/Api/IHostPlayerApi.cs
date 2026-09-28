namespace Octaryn.Shared.Host.Api;

// Authoritative player channel. The host owns storage, cadence and
// persistence; a module that requests host.player drives the state each
// player step and reads back exactly what the host will replicate.
public interface IHostPlayerApi
{
    // Current authoritative state. False when no player exists yet.
    bool TryGetState(out HostCharacterState state);

    // Writes the module-computed authoritative state for this player step.
    void SetState(in HostCharacterState state);
}
