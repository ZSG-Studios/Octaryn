using System.Runtime.InteropServices;

namespace Octaryn.Server.Host;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeHostStartupPolicy(uint disableGameModules, uint liveProcessStream, uint liveStreamIntervalMilliseconds)
{
    private readonly uint _disableGameModules = disableGameModules;
    private readonly uint _liveProcessStream = liveProcessStream;

    public readonly uint LiveStreamIntervalMilliseconds = liveStreamIntervalMilliseconds;

    public bool DisableGameModules => _disableGameModules != 0;
    public bool LiveProcessStream => _liveProcessStream != 0;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeHostLiveStreamPaths(
    IntPtr chunkViewIntentPath,
    IntPtr chunkStreamPath,
    IntPtr playerInputIntentPath,
    IntPtr blockInteractionIntentPath,
    IntPtr worldTimeIntentPath,
    uint metadataOnly)
{
    private readonly IntPtr _chunkViewIntentPath = chunkViewIntentPath;
    private readonly IntPtr _chunkStreamPath = chunkStreamPath;
    private readonly IntPtr _playerInputIntentPath = playerInputIntentPath;
    private readonly IntPtr _blockInteractionIntentPath = blockInteractionIntentPath;
    private readonly IntPtr _worldTimeIntentPath = worldTimeIntentPath;
    private readonly uint _metadataOnly = metadataOnly;

    public string? ChunkViewIntentPath => NativeString(_chunkViewIntentPath);
    public string? ChunkStreamPath => NativeString(_chunkStreamPath);
    public string? PlayerInputIntentPath => NativeString(_playerInputIntentPath);
    public string? BlockInteractionIntentPath => NativeString(_blockInteractionIntentPath);
    public string? WorldTimeIntentPath => NativeString(_worldTimeIntentPath);
    public bool MetadataOnly => _metadataOnly != 0;

    private static string? NativeString(IntPtr value)
    {
        return value == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(value);
    }
}
