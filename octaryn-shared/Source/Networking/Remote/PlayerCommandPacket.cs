using System.Buffers.Binary;
using System.Text.Json;

namespace Octaryn.Shared.Networking.Remote;

public readonly record struct PlayerCommand(
    ulong FrameIndex, uint Flags, uint Controller,
    float MoveX, float MoveY, float MoveZ, float CameraPitch, float CameraYaw, int RelativeMouse);

public static class PlayerCommandPacket
{
    public const byte Header = 0x50;
    public const int HeaderSize = 4;
    public const int CommandSize = 40;
    public const int MaxBatch = 64;
    public const int MaxDatagramCommands = 20;

    public static bool Valid(in PlayerCommand command) =>
        command.FrameIndex != 0 && (command.Flags & ~31u) == 0 && command.Controller <= 1 &&
        command.RelativeMouse is >= 0 and <= 1 &&
        float.IsFinite(command.MoveX) && Math.Abs(command.MoveX) <= 1 &&
        float.IsFinite(command.MoveY) && Math.Abs(command.MoveY) <= 1 &&
        float.IsFinite(command.MoveZ) && Math.Abs(command.MoveZ) <= 1 &&
        float.IsFinite(command.CameraPitch) && float.IsFinite(command.CameraYaw);

    public static PlayerCommand[] ReadJson(ReadOnlyMemory<byte> bytes)
    {
        if (bytes.Length > RemoteProtocol.MaxIntentTextBytes) return [];
        try
        {
            using var document = JsonDocument.Parse(bytes);
            var root = document.RootElement;
            if (root.GetProperty("version").GetInt32() != 2) return [];
            var commands = root.GetProperty("commands");
            if (commands.ValueKind != JsonValueKind.Array || commands.GetArrayLength() > MaxBatch) return [];
            var result = new PlayerCommand[commands.GetArrayLength()];
            ulong previous = 0;
            var index = 0;
            foreach (var value in commands.EnumerateArray())
            {
                var command = new PlayerCommand(value.GetProperty("frameIndex").GetUInt64(),
                    value.GetProperty("flags").GetUInt32(), value.GetProperty("controller").GetUInt32(),
                    value.GetProperty("moveX").GetSingle(), value.GetProperty("moveY").GetSingle(),
                    value.GetProperty("moveZ").GetSingle(), value.GetProperty("cameraPitch").GetSingle(),
                    value.GetProperty("cameraYaw").GetSingle(), value.GetProperty("relativeMouse").GetInt32());
                if (!Valid(in command) || (previous != 0 && command.FrameIndex != previous + 1)) return [];
                result[index++] = command;
                previous = command.FrameIndex;
            }
            return result;
        }
        catch (Exception error) when (error is JsonException or InvalidOperationException or
            KeyNotFoundException or FormatException or OverflowException)
        {
            return [];
        }
    }

    public static byte[] Encode(ReadOnlySpan<PlayerCommand> commands)
    {
        if (commands.Length is < 1 or > MaxDatagramCommands) throw new ArgumentOutOfRangeException(nameof(commands));
        var bytes = new byte[HeaderSize + CommandSize * commands.Length];
        bytes[0] = Header;
        bytes[1] = (byte)RemoteProtocol.Version;
        BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(2), (ushort)commands.Length);
        for (var i = 0; i < commands.Length; i++)
        {
            var command = commands[i];
            if (!Valid(in command)) throw new ArgumentException("Invalid player command", nameof(commands));
            var target = bytes.AsSpan(HeaderSize + i * CommandSize, CommandSize);
            BinaryPrimitives.WriteUInt64LittleEndian(target, command.FrameIndex);
            BinaryPrimitives.WriteUInt32LittleEndian(target[8..], command.Flags);
            BinaryPrimitives.WriteUInt32LittleEndian(target[12..], command.Controller);
            BinaryPrimitives.WriteSingleLittleEndian(target[16..], command.MoveX);
            BinaryPrimitives.WriteSingleLittleEndian(target[20..], command.MoveY);
            BinaryPrimitives.WriteSingleLittleEndian(target[24..], command.MoveZ);
            BinaryPrimitives.WriteSingleLittleEndian(target[28..], command.CameraPitch);
            BinaryPrimitives.WriteSingleLittleEndian(target[32..], command.CameraYaw);
            BinaryPrimitives.WriteInt32LittleEndian(target[36..], command.RelativeMouse);
        }
        return bytes;
    }

    public static PlayerCommand[] Decode(ReadOnlySpan<byte> bytes)
    {
        if (bytes.Length < HeaderSize || bytes[0] != Header || bytes[1] != RemoteProtocol.Version) return [];
        var count = BinaryPrimitives.ReadUInt16LittleEndian(bytes[2..]);
        if (count is < 1 or > MaxDatagramCommands || bytes.Length != HeaderSize + count * CommandSize) return [];
        var commands = new PlayerCommand[count];
        ulong previous = 0;
        for (var i = 0; i < count; i++)
        {
            var source = bytes.Slice(HeaderSize + i * CommandSize, CommandSize);
            var command = new PlayerCommand(BinaryPrimitives.ReadUInt64LittleEndian(source),
                BinaryPrimitives.ReadUInt32LittleEndian(source[8..]), BinaryPrimitives.ReadUInt32LittleEndian(source[12..]),
                BinaryPrimitives.ReadSingleLittleEndian(source[16..]), BinaryPrimitives.ReadSingleLittleEndian(source[20..]),
                BinaryPrimitives.ReadSingleLittleEndian(source[24..]), BinaryPrimitives.ReadSingleLittleEndian(source[28..]),
                BinaryPrimitives.ReadSingleLittleEndian(source[32..]), BinaryPrimitives.ReadInt32LittleEndian(source[36..]));
            if (!Valid(in command) || (previous != 0 && command.FrameIndex != previous + 1)) return [];
            commands[i] = command;
            previous = command.FrameIndex;
        }
        return commands;
    }
}
