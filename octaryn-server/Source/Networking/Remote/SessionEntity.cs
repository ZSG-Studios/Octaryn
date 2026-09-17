using LiteEntitySystem;

namespace Octaryn.Shared.Networking.Remote;

// LiteEntitySystem session entity: one per attached remote player, owned by the
// server. SyncVars carry the latest authoritative pose at the server send rate;
// the client's native PoseHistory owns interpolation, so no LES interpolation
// flags are used. RemoteCall channels carry chunk snapshots, block
// acknowledgements and the welcome handshake response. The hello request and
// client intents arrive through the player-owned SessionController instead.
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

    private static RemoteCallSpan<byte> _snapshotRpc;
    private static RemoteCall<ulong> _blockAckRpc;
    private static RemoteCall<ulong> _welcomeRpc;
    private static RemoteCallSpan<byte> _itemSnapshotRpc;

    public SessionEntity(EntityParams parameters) : base(parameters)
    {
    }

    protected override void RegisterRPC(ref RPCRegistrator r)
    {
        base.RegisterRPC(ref r);
        r.CreateRPCAction(this, (SpanAction<byte>)OnSnapshot, ref _snapshotRpc, ExecuteFlags.SendToAll);
        r.CreateRPCAction(this, (Action<ulong>)OnBlockAck, ref _blockAckRpc, ExecuteFlags.SendToAll);
        r.CreateRPCAction(this, (Action<ulong>)OnWelcome, ref _welcomeRpc, ExecuteFlags.SendToAll);
        r.CreateRPCAction(this, (SpanAction<byte>)OnItemSnapshot, ref _itemSnapshotRpc, ExecuteFlags.SendToAll);
    }

    public void PublishPose(ulong frameIndex, ulong acknowledgedInputFrame, ulong sourceTick, double sourceSeconds,
        float x, float y, float z, float pitch, float yaw,
        float velocityX, float velocityY, float velocityZ,
        bool onGround, bool flying, float worldDayFraction, double worldTotalSeconds)
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
        _stateFlags.Value = (onGround ? 1u : 0u) | (flying ? 2u : 0u) | 4u;
        _sourceTick.Value = sourceTick;
        _sourceSeconds.Value = sourceSeconds;
        _worldTotalSeconds.Value = worldTotalSeconds;
        _acknowledgedInputFrame.Value = acknowledgedInputFrame;
        _frameIndex.Value = frameIndex;
    }

    public void SendWelcome(ulong version) => ExecuteRPC(_welcomeRpc, version);
    public void SendSnapshot(ReadOnlySpan<byte> payload) => ExecuteRPC(_snapshotRpc, payload);
    public void SendItemSnapshot(ReadOnlySpan<byte> payload) => ExecuteRPC(_itemSnapshotRpc, payload);
    public void SendBlockAck(ulong frameIndex) => ExecuteRPC(_blockAckRpc, frameIndex);

    private void OnItemSnapshot(ReadOnlySpan<byte> payload)
    {
    }

    private void OnSnapshot(ReadOnlySpan<byte> payload)
    {
    }

    private void OnBlockAck(ulong frameIndex)
    {
    }

    private void OnWelcome(ulong version)
    {
    }
}
