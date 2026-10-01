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

    private static RemoteCall<SessionWelcome> _welcomeRpc;
    private static RemoteCall<SessionEventEnvelope> _moduleEventRpc;

    public event Action<SessionWelcome>? WelcomeReceived;

    // Module broadcast from the authority: event id plus three payload words.
    public event Action<SessionEventEnvelope>? ModuleEventReceived;

    public SessionEntity(EntityParams parameters) : base(parameters)
    {
    }

    protected override void RegisterRPC(ref RPCRegistrator r)
    {
        base.RegisterRPC(ref r);
        r.CreateRPCAction(this, (Action<SessionWelcome>)OnWelcome, ref _welcomeRpc, ExecuteFlags.SendToAll);
        r.CreateRPCAction(this, (Action<SessionEventEnvelope>)OnModuleEvent, ref _moduleEventRpc, ExecuteFlags.SendToAll);
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
 JumpHeld = (_stateFlags.Value & 8u) != 0,
            WorldDayFraction = _worldDayFraction.Value,
            WorldTotalSeconds = _worldTotalSeconds.Value,
        };
        return true;
    }

    private void OnWelcome(SessionWelcome version)
    {
        WelcomeReceived?.Invoke(version);
    }

    private void OnModuleEvent(SessionEventEnvelope data)
    {
        ModuleEventReceived?.Invoke(data);
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
 public bool JumpHeld;
    public float WorldDayFraction;
    public double WorldTotalSeconds;
}
