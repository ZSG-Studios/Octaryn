using LiteEntitySystem;
using LiteNetLib.Utils;

namespace Octaryn.Shared.Networking.Remote;

// Client-to-server hello handshake: protocol version for the session.
public struct SessionHelloRequest : INetSerializable
{
    public ulong Version;
    public ulong SessionHigh;
    public ulong SessionLow;

    public readonly void Serialize(NetDataWriter writer)
    { writer.Put(Version); writer.Put(SessionHigh); writer.Put(SessionLow); }
    public void Deserialize(NetDataReader reader)
    { Version = reader.GetULong(); SessionHigh = reader.GetULong(); SessionLow = reader.GetULong(); }
}

// Client-to-server intent frame: kind byte plus the mailbox payload. Text
// intents carry UTF8 JSON; world item intents carry the raw binary record.
public struct SessionIntentRequest : INetSerializable
{
    public byte Kind;
    public byte[] Payload;

    public readonly void Serialize(NetDataWriter writer)
    {
        writer.Put(Kind);
        writer.PutBytesWithLength(Payload ?? System.Array.Empty<byte>());
    }

    public void Deserialize(NetDataReader reader)
    {
        Kind = reader.GetByte();
        Payload = reader.GetBytesWithLength();
    }
}

// Minimal input placeholder for the unused LES input pipeline.
public struct SessionInput
{
    public byte Value;
}

// Player-owned controller: carries the hello handshake and non-movement intents to
// the server through the LES request channel. The LES input pipeline stays
// unused; the native session kernel owns movement authority and cadence.
// octaryn-client compiles an identical wire copy; keep both in sync.
public sealed class SessionController : HumanControllerLogic<SessionInput>
{
    public event System.Action<ulong, ulong, ulong>? HelloReceived;
    public event System.Action<byte, byte[]>? IntentReceived;

    public SessionController(EntityParams parameters) : base(parameters)
    {
        if (EntityManager.IsServer)
        {
            SubscribeToClientRequestStruct<SessionHelloRequest>(
                request => HelloReceived?.Invoke(request.Version, request.SessionHigh, request.SessionLow));
            SubscribeToClientRequestStruct<SessionIntentRequest>(
                request => IntentReceived?.Invoke(request.Kind, request.Payload));
        }
    }

    public void SendHello(ulong version, ulong high, ulong low)
    {
        if (EntityManager.IsClient)
        {
            SendRequestStruct(new SessionHelloRequest { Version = version, SessionHigh = high, SessionLow = low });
        }
    }

    public void SendIntent(byte kind, byte[] payload)
    {
        if (EntityManager.IsClient)
        {
            SendRequestStruct(new SessionIntentRequest { Kind = kind, Payload = payload });
        }
    }
}
