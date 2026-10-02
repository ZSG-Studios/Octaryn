namespace Octaryn.Shared.Host.Api;

public enum HostRegionPhase : uint { Absent, Loading, Prepared, Uploading, Ray, Ready }
[Flags] public enum HostRegionFlags : uint { Wanted=1, Retained=2, RenderReady=4, CollisionReady=8, Failed=16 }
public readonly record struct HostRegionAnchor(float X,float Y,float Z);
public readonly record struct HostRegionBounds(float MinX,float MinY,float MinZ,float MaxX,float MaxY,float MaxZ);
public readonly record struct HostRegionResidency(uint Index,string Id,HostRegionPhase Phase,HostRegionFlags Flags,
    ulong Generation,HostRegionBounds Bounds);
// Actual native tile publication and collision state. Absence means this scene
// does not use identified tile streaming; CPU scene preparation is separate.
public interface IHostResidencyApi
{
    bool TryGetActorPosition(out HostRegionAnchor position);
    bool TryGetCount(out uint count,out ulong generation);
    bool TryGetRegion(uint index,out HostRegionResidency region);
    bool SetDesiredRegions(ReadOnlySpan<uint> wanted,ReadOnlySpan<uint> retained);
}
