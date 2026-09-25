using System.Runtime.InteropServices;

namespace Octaryn.Server.Host;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeHostStartupPolicy(uint liveProcessStream, uint liveStreamIntervalMilliseconds)
{
    private readonly uint _liveProcessStream = liveProcessStream;

    public readonly uint LiveStreamIntervalMilliseconds = liveStreamIntervalMilliseconds;

    public bool LiveProcessStream => _liveProcessStream != 0;
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeHostLiveStreamPaths(
    IntPtr chunkViewIntentPath,
    IntPtr playerInputIntentPath,
    IntPtr playerStateStreamPath,
    IntPtr worldTimeIntentPath)
{
    private readonly IntPtr _chunkViewIntentPath = chunkViewIntentPath;
    private readonly IntPtr _playerInputIntentPath = playerInputIntentPath;
    private readonly IntPtr _playerStateStreamPath = playerStateStreamPath;
    private readonly IntPtr _worldTimeIntentPath = worldTimeIntentPath;

    public string? ChunkViewIntentPath => NativeString(_chunkViewIntentPath);
    public string? PlayerInputIntentPath => NativeString(_playerInputIntentPath);
    public string? PlayerStateStreamPath => NativeString(_playerStateStreamPath);
    public string? WorldTimeIntentPath => NativeString(_worldTimeIntentPath);

    private static string? NativeString(IntPtr value)
    {
        return value == IntPtr.Zero ? null : Marshal.PtrToStringUTF8(value);
    }
}

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeHostLiveStreamRequestPlan(uint shouldHandle, uint shouldContinue, int handleResult, uint reason)
{
    private readonly uint _shouldHandle = shouldHandle;
    private readonly uint _shouldContinue = shouldContinue;

    public readonly int HandleResult = handleResult;
    public readonly uint Reason = reason;

    public bool ShouldHandle => _shouldHandle != 0;
    public bool ShouldContinue => _shouldContinue != 0;
}
