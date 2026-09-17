namespace Octaryn.Server.Networking.Remote;

// Arch ECS components for the single authoritative remote session. The session
// state that used to be loose fields lives on one Arch entity per attachment;
// the session systems query and mutate these components each server step.

internal struct SessionConnectionComponent
{
    public bool Welcomed;
    public string Label;
}

internal struct SessionIntentComponent
{
    public readonly Dictionary<Octaryn.Shared.Networking.Remote.RemoteIntentKind, byte[]> PendingIntents;

    public SessionIntentComponent()
    {
        PendingIntents = new Dictionary<Octaryn.Shared.Networking.Remote.RemoteIntentKind, byte[]>();
    }
}

internal struct SessionPublishComponent
{
    public byte[]? LastPoseBytes;
    public byte[]? LastSnapshotBytes;
    public ulong PendingBlockAck;
}
