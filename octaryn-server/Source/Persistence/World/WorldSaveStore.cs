using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;
using Octaryn.Shared.GameModules;

namespace Octaryn.Server.Persistence.World;

internal sealed class WorldSaveStore(string directory)
{
    internal const int MaximumFileBytes = 2 * 1024 * 1024;
    private const uint Magic = 0x5653475a;
    private const int HeaderBytes = 48;
    private static readonly JsonSerializerOptions s_json = new() { UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow };
    internal string Path { get; } = System.IO.Path.Combine(directory, "world-state.save");

    public WorldSaveRecord? Read(string moduleId, string compatibilityId)
    {
        if (!File.Exists(Path)) return null;
        using var stream = new FileStream(Path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        if (stream.Length < HeaderBytes || stream.Length > MaximumFileBytes)
            throw new InvalidDataException("World save is truncated or exceeds the size limit.");
        Span<byte> header = stackalloc byte[HeaderBytes]; stream.ReadExactly(header);
        if (BinaryPrimitives.ReadUInt32LittleEndian(header) != Magic ||
            BinaryPrimitives.ReadUInt32LittleEndian(header[4..]) != 1 ||
            BinaryPrimitives.ReadUInt32LittleEndian(header[8..]) != stream.Length - HeaderBytes ||
            BinaryPrimitives.ReadUInt32LittleEndian(header[12..]) != 0)
            throw new InvalidDataException("World save header/version is invalid.");
        var payload = new byte[(int)stream.Length - HeaderBytes]; stream.ReadExactly(payload);
        if (!CryptographicOperations.FixedTimeEquals(header[16..], SHA256.HashData(payload)))
            throw new InvalidDataException("World save integrity check failed.");
        var record = JsonSerializer.Deserialize<WorldSaveRecord>(payload, s_json)
            ?? throw new InvalidDataException("World save is empty.");
        Validate(record);
        if (record.ModuleId != moduleId || record.CompatibilityId != compatibilityId)
            throw new InvalidDataException("World save module compatibility does not match the active game.");
        return record;
    }

    public void Write(WorldSaveRecord record)
    {
        Validate(record);
        var payload = JsonSerializer.SerializeToUtf8Bytes(record, s_json);
        if (payload.Length > MaximumFileBytes - HeaderBytes) throw new InvalidDataException("World save size limit exceeded.");
        Span<byte> header = stackalloc byte[HeaderBytes]; header.Clear();
        BinaryPrimitives.WriteUInt32LittleEndian(header, Magic);
        BinaryPrimitives.WriteUInt32LittleEndian(header[4..], 1);
        BinaryPrimitives.WriteUInt32LittleEndian(header[8..], (uint)payload.Length);
        SHA256.HashData(payload).CopyTo(header[16..]);
        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(Path)!);
        var temporary = Path + ".tmp";
        using (var stream = new FileStream(temporary, FileMode.Create, FileAccess.Write, FileShare.None, 65536, FileOptions.WriteThrough))
        {
            stream.Write(header); stream.Write(payload); stream.Flush(flushToDisk: true);
        }
        if (File.Exists(Path)) File.Replace(temporary, Path, null);
        else File.Move(temporary, Path);
    }

    private static void Validate(WorldSaveRecord record)
    {
        if (record.Version != 1 || record.Generation == 0 || record.SessionId is 0 or uint.MaxValue || string.IsNullOrEmpty(record.ModuleId) ||
            string.IsNullOrEmpty(record.CompatibilityId) || record.ModuleId.Length > 160 || record.CompatibilityId.Length > 160 ||
            record.ModuleState is null || record.ModuleState.Length is 0 or > IGameModuleSaveState.MaximumBytes ||
            record.Player is null || record.Clock is null)
            throw new InvalidDataException("World save metadata is invalid.");
        var player = record.Player;
        if (!float.IsFinite(player.X) || !float.IsFinite(player.Y) || !float.IsFinite(player.Z) ||
            !float.IsFinite(player.Pitch) || !float.IsFinite(player.Yaw))
            throw new InvalidDataException("World save player pose is invalid.");
        var clock = record.Clock;
        if (clock.Version != 1 || !double.IsFinite(clock.SecondsOfDay) || clock.SecondsOfDay < 0 || clock.SecondsOfDay >= 86400 ||
            !double.IsFinite(clock.SpeedMultiplier) || clock.SpeedMultiplier < 0 || clock.SpeedMultiplier > 1000000)
            throw new InvalidDataException("World save clock is invalid.");
    }
}
