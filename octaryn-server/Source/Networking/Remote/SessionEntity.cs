using LiteEntitySystem;

namespace Octaryn.Shared.Networking.Remote;

// LiteEntitySystem session entity: one per attached remote player, owned by the
// server. SyncVars carry the latest authoritative pose at the server send rate;
// native owning-player prediction consumes coherent raw state, so no LES
// interpolation flags are used. The welcome handshake rides a RemoteCall; the
// hello request and client intents arrive through the player-owned
// SessionController instead.
//
// octaryn-client compiles an identical wire copy of this entity. Field
// declaration order and RegisterRPC call order must stay identical on both
// sides: LES assigns sync field and RPC ids by registration order.
public sealed class SessionEntity : EntityLogic
{
    [SyncVarFlags(SyncFlags.None)] private SyncVar<float> _posX;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<float> _posY;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<float> _posZ;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<float> _pitch;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<float> _yaw;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<float> _velocityX;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<float> _velocityY;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<float> _velocityZ;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<float> _worldDayFraction;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<uint> _stateFlags;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<ulong> _sourceTick;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<double> _sourceSeconds;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<double> _worldTotalSeconds;
    [SyncVarFlags(SyncFlags.None)] private SyncVar<ulong> _acknowledgedInputFrame;
    // Batch marker: always written last for each published pose.
    [SyncVarFlags(SyncFlags.None)] private SyncVar<ulong> _frameIndex;

    private static RemoteCall<SessionWelcome> _welcomeRpc;
    private static RemoteCall<SessionEventEnvelope> _moduleEventRpc;

    public SessionEntity(EntityParams parameters) : base(parameters)
    {
    }

    protected override void RegisterRPC(ref RPCRegistrator r)
    {
        base.RegisterRPC(ref r);
        r.CreateRPCAction(this, (Action<SessionWelcome>)OnWelcome, ref _welcomeRpc, ExecuteFlags.SendToAll);
        r.CreateRPCAction(this, (Action<SessionEventEnvelope>)OnModuleEvent, ref _moduleEventRpc, ExecuteFlags.SendToAll);
    }

    public void PublishPose(ulong frameIndex, ulong acknowledgedInputFrame, ulong sourceTick, double sourceSeconds,
        float x, float y, float z, float pitch, float yaw,
        float velocityX, float velocityY, float velocityZ,
 bool onGround, bool flying, float worldDayFraction, double worldTotalSeconds, bool jumpHeld = false)
    {
        _posX.Value = x;
        _posY.Value = y;
        _posZ.Value = z;
        _pitch.Value = pitch;
        _yaw.Value = yaw;
        _velocityX.Value = velocityX;
        _velocityY.Value = velocityY;
        _velocityZ.Value = velocityZ;
        _worldDayFraction.Value = worldDayFraction;
        // Bit 2 marks a published pose; the first pose legitimately has frameIndex 0.
 _stateFlags.Value = (onGround ? 1u : 0u) | (flying ? 2u : 0u) | 4u | (jumpHeld ? 8u : 0u);
        _sourceTick.Value = sourceTick;
        _sourceSeconds.Value = sourceSeconds;
        _worldTotalSeconds.Value = worldTotalSeconds;
        _acknowledgedInputFrame.Value = acknowledgedInputFrame;
        _frameIndex.Value = frameIndex;
    }

    public void SendWelcome(SessionWelcome version) => ExecuteRPC(_welcomeRpc, version);

    // Module broadcast, server-owned; the client never calls this.
    public void BroadcastModuleEvent(in SessionEventEnvelope data) => ExecuteRPC(_moduleEventRpc, data);

    private void OnWelcome(SessionWelcome version)
    {
    }

    private void OnModuleEvent(SessionEventEnvelope data)
    {
    }
}
