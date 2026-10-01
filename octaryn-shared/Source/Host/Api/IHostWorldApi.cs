namespace Octaryn.Shared.Host.Api;

public interface IHostWorldApi
{
    bool IsActive { get; }

    ulong TriangleCount { get; }

    bool TryGetSpawnPose(out HostSpawnPose pose);
}
