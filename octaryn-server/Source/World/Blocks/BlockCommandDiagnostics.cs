using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Blocks;

internal static class BlockCommandDiagnostics
{
    public static string EditLabel(HostCommand command)
    {
        if (command.Kind != HostCommandKind.SetBlock)
        {
            return "none";
        }

        return command.D == BlockId.Air.Value ? "break" : "place";
    }
}
