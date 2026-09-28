namespace Octaryn.Shared.GameModules;

using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;

public readonly record struct ModuleHostContext(
    IHostCommandSink Commands,
    IHostTimeApi? Time,
    IHostDiagnosticsApi? Diagnostics,
    IHostPhysicsApi? Physics,
    IHostWorldApi? World,
    IHostPlayerApi? Player,
    IHostEcsApi? Ecs,
    IHostInputApi? Input,
    IHostSchedulingApi? Scheduling,
    IHostAudioApi? Audio,
    IHostUiApi? Ui,
    IHostReplicationApi? Replication);
