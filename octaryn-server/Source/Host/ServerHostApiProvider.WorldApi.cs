using Octaryn.Server.World.MapWorld;
using Octaryn.Shared.Host.Api;

namespace Octaryn.Server.Host;

internal sealed partial class ServerHostApiProvider
{
    private sealed class WorldApi : IHostWorldApi
    {
        private readonly IntPtr _mapWorld;

        public WorldApi(IntPtr mapWorld)
        {
            _mapWorld = mapWorld;
        }

        public bool IsActive => true;

        public ulong TriangleCount => NativeMapWorld.TriangleCount(_mapWorld);

        public bool TryGetSpawnPose(out HostSpawnPose pose)
        {
            var state = NativeMapWorld.SpawnState(_mapWorld);
            pose = new HostSpawnPose(state.X, state.Y, state.Z, state.Yaw, state.Pitch);
            return true;
        }
    }
}
