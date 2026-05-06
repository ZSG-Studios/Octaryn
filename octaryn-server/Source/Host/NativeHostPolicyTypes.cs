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
