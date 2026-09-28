using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Shared.Networking.Remote;

namespace Octaryn.Client.HostBridge;

[StructLayout(LayoutKind.Sequential)]
internal struct RemoteCommandWire
{
    public ulong Frame;
    public uint Flags, Controller;
    public float X, Y, Z, Pitch, Yaw;
    public int RelativeMouse;
}

[StructLayout(LayoutKind.Sequential)]
internal struct RemotePoseWire
{
    public uint Version, Size;
    public ulong Ack, Tick;
    public double Seconds, WorldSeconds;
    public float X, Y, Z, Pitch, Yaw, VelocityX, VelocityY, VelocityZ, DayFraction;
    public uint Flags;
}

internal static partial class HostExports
{
    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemoteSubmitIntent(byte kind, byte* source, int length)
    {
        if (source is null || length is < 1 or > 65536) return -1;
        var transport = Volatile.Read(ref s_remoteTransport);
        return transport is not null && transport.SubmitIntent(kind, new ReadOnlySpan<byte>(source, length).ToArray()) ? 0 : -2;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemotePollActionAck(ulong* epoch, ulong* sequence)
    {
        if (epoch is null || sequence is null) return -1;
        var transport = Volatile.Read(ref s_remoteTransport);
        if (transport is null || !transport.TryPollActionAcknowledgement(out var receivedEpoch, out var receivedSequence)) return 0;
        *epoch = receivedEpoch;
        *sequence = receivedSequence;
        return 1;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemoteSubmitCommands(RemoteCommandWire* source, int count, int stride)
    {
        if (source is null || count is < 1 or > PlayerCommandPacket.MaxBatch || stride != sizeof(RemoteCommandWire)) return -1;
        var transport = Volatile.Read(ref s_remoteTransport);
        if (transport is null) return -2;
        var commands = new PlayerCommand[count];
        for (var i = 0; i < count; ++i)
        {
            var value = source[i];
            commands[i] = new PlayerCommand(value.Frame, value.Flags, value.Controller,
                value.X, value.Y, value.Z, value.Pitch, value.Yaw, value.RelativeMouse);
        }
        return transport.SubmitCommands(commands) ? 0 : -3;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static unsafe int RemotePollPose(RemotePoseWire* output)
    {
        if (output is null || output->Version != 1 || output->Size != sizeof(RemotePoseWire)) return -1;
        var transport = Volatile.Read(ref s_remoteTransport);
        if (transport is null || !transport.TryPollPose(out var pose)) return 0;
        *output = new RemotePoseWire {
            Version = 1, Size = (uint)sizeof(RemotePoseWire), Ack = pose.AcknowledgedInputFrame,
            Tick = pose.SourceTick, Seconds = pose.SourceSeconds, WorldSeconds = pose.WorldTotalSeconds,
            X = pose.X, Y = pose.Y, Z = pose.Z, Pitch = pose.Pitch, Yaw = pose.Yaw,
            VelocityX = pose.VelocityX, VelocityY = pose.VelocityY, VelocityZ = pose.VelocityZ,
            DayFraction = pose.WorldDayFraction,
            Flags = (pose.OnGround ? 1u : 0) | (pose.Flying ? 2u : 0) | (pose.JumpHeld ? 4u : 0)
        };
        return 1;
    }
}
