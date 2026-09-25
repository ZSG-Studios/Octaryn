namespace Octaryn.Shared.Networking.Remote;

// Wire message kinds for the host-owned remote session transport. The first
// payload byte of every packet selects the kind; remaining bytes are the
// kind-specific payload. Only shapes and identifiers live here: transport
// implementations stay in octaryn-client and octaryn-server.
public enum RemoteMessageKind : byte
{
    None = 0,
    Hello = 1,
    Welcome = 2,
    Reject = 3,
    Intent = 4,
    PlayerState = 5,
    ChunkSnapshot = 6,
    BlockAck = 7,
    Goodbye = 8,
}

// Client-to-server intent payload selectors carried by Intent messages.
public enum RemoteIntentKind : byte
{
    None = 0,
    ChunkView = 1,
    PlayerInput = 2,
    BlockInteraction = 3,
    WorldTime = 4,
    WorldItems = 5,
    BlockResultsAck = 6,
}

// LiteEntitySystem entity class ids for the remote session transport. Both
// hosts register identical ids so the replicated entity maps line up.
public enum RemoteEntityType : ushort
{
    None = 0,
    Session = 1,
    Controller = 2
}

public static class RemoteProtocol
{
 public const uint Version = 5u;
 public const string ConnectionKey = "octaryn-remote-v4";
    public const int DefaultPort = 17531;
    public const int MaxIntentTextBytes = 131072;
    public const int MaxRejectReasonChars = 256;
}
