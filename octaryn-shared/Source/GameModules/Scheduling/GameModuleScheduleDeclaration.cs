using Octaryn.Shared.Host;

namespace Octaryn.Shared.GameModules;

public sealed record GameModuleScheduleDeclaration(
    IReadOnlyList<ScheduledSystemDeclaration> Systems);
