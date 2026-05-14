using System.Runtime.InteropServices;

namespace Octaryn.Engine.Api;

[StructLayout(LayoutKind.Sequential, Size = 88)]
public readonly struct OctarynWorldSnapshot
{
    public const uint VersionValue = 1;

    public readonly uint Version;
    public readonly uint Size;
    public readonly ulong FrameIndex;
    public readonly ulong DayIndex;
    public readonly double DeltaSeconds;
    public readonly double TotalWorldSeconds;
    public readonly uint SecondOfDay;
    public readonly int PlayerBlockX;
    public readonly int PlayerBlockY;
    public readonly int PlayerBlockZ;
    public readonly int RenderDistance;
    public readonly int ActiveChunks;
    public readonly int LoadedChunks;
    public readonly int MeshReadyChunks;
    public readonly int RunningJobs;
    public readonly int PendingWindowTransition;
    public readonly int Reserved;
}
