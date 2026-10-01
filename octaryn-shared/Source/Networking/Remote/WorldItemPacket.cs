using System.Buffers.Binary;
using System.Runtime.InteropServices;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Shared.Networking.Remote;

public static class WorldItemPacket
{
    public const byte Header = 0x49, AckHeader = 0x4a;
    public const byte BeginBaseline = 1, EndBaseline = 2, EndBatch = 4;
    public const int HeaderBytes = 24, PoseBytes = 64, MaximumPoses = 16, MaximumItems = 10000;

    public static byte[] Encode(ulong epoch, ulong sequence, byte flags, ReadOnlySpan<HostWorldItemPose> poses)
    {
        if (!BitConverter.IsLittleEndian || poses.Length > MaximumPoses || epoch == 0 || sequence == 0)
            throw new ArgumentException("Invalid item pose batch");
        var bytes = new byte[HeaderBytes + poses.Length * PoseBytes];
        bytes[0] = Header; bytes[1] = 1; bytes[2] = flags;
        BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(4), epoch);
        BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(12), sequence);
        BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(20), poses.Length);
        MemoryMarshal.AsBytes(poses).CopyTo(bytes.AsSpan(HeaderBytes));
        return bytes;
    }

    public static bool Decode(ReadOnlySpan<byte> bytes, out ulong epoch, out ulong sequence,
        out byte flags, out ReadOnlySpan<HostWorldItemPose> poses)
    {
        epoch = sequence = 0; flags = 0; poses = default;
        if (!BitConverter.IsLittleEndian || bytes.Length < HeaderBytes || bytes[0] != Header ||
            bytes[1] != 1 || bytes[3] != 0 || (bytes[2] & ~7) != 0) return false;
        var count = BinaryPrimitives.ReadInt32LittleEndian(bytes[20..]);
        if (count is < 0 or > MaximumPoses || bytes.Length != HeaderBytes + count * PoseBytes) return false;
        epoch = BinaryPrimitives.ReadUInt64LittleEndian(bytes[4..]);
        sequence = BinaryPrimitives.ReadUInt64LittleEndian(bytes[12..]);
        flags = bytes[2];
        if (epoch == 0 || sequence == 0) return false;
        poses = MemoryMarshal.Cast<byte, HostWorldItemPose>(bytes[HeaderBytes..]);
        foreach (ref readonly var pose in poses) if (!pose.IsValid || pose.Generation == 0) return false;
        return true;
    }

    public static byte[] Acknowledge(ulong epoch, ulong sequence)
    {
        var bytes = new byte[20]; bytes[0] = AckHeader; bytes[1] = 1;
        BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(4), epoch);
        BinaryPrimitives.WriteUInt64LittleEndian(bytes.AsSpan(12), sequence);
        return bytes;
    }
}
