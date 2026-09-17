using LiteEntitySystem;

namespace Octaryn.Shared.Networking.Remote;

// LiteEntitySystem session entity: one per attached remote player, owned by the
// server. This is the client-side wire copy of the entity defined in
// octaryn-server/Source/Networking/Remote/SessionEntity.cs. Field declaration
// order and RegisterRPC call order must stay identical on both sides: LES
// assigns sync field and RPC ids by registration order.
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

    public event Action<ulong>? WelcomeReceived;
    public event Action<byte[]>? SnapshotReceived;
    public event Action<byte[]>? ItemSnapshotReceived;
    public event Action<ulong>? BlockAckReceived;

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

    public bool TryReadPose(out SessionPose pose)
    {
        pose = default;
        // Bit 2 marks a published pose; the first pose legitimately has frameIndex 0.
        if ((_stateFlags.Value & 4u) == 0)
        {
            return false;
        }

        pose = new SessionPose
        {
            FrameIndex = _frameIndex.Value,
            AcknowledgedInputFrame = _acknowledgedInputFrame.Value,
            SourceTick = _sourceTick.Value,
            SourceSeconds = _sourceSeconds.Value,
            X = _posX.Value,
            Y = _posY.Value,
            Z = _posZ.Value,
            Pitch = _pitch.Value,
            Yaw = _yaw.Value,
            VelocityX = _velocityX.Value,
            VelocityY = _velocityY.Value,
            VelocityZ = _velocityZ.Value,
            OnGround = (_stateFlags.Value & 1u) != 0,
            Flying = (_stateFlags.Value & 2u) != 0,
            WorldDayFraction = _worldDayFraction.Value,
            WorldTotalSeconds = _worldTotalSeconds.Value,
        };
        return true;
    }

    private void OnSnapshot(ReadOnlySpan<byte> payload)
    {
        SnapshotReceived?.Invoke(payload.ToArray());
    }

    private void OnBlockAck(ulong frameIndex)
    {
        BlockAckReceived?.Invoke(frameIndex);
    }

    private void OnWelcome(ulong version)
    {
        WelcomeReceived?.Invoke(version);
    }

    private void OnItemSnapshot(ReadOnlySpan<byte> payload)
    {
        ItemSnapshotReceived?.Invoke(payload.ToArray());
    }
}

public struct SessionPose
{
    public ulong FrameIndex;
    public ulong AcknowledgedInputFrame;
    public ulong SourceTick;
    public double SourceSeconds;
    public float X;
    public float Y;
    public float Z;
    public float Pitch;
    public float Yaw;
    public float VelocityX;
    public float VelocityY;
    public float VelocityZ;
    public bool OnGround;
    public bool Flying;
    public float WorldDayFraction;
    public double WorldTotalSeconds;
}
