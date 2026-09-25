namespace Octaryn.Shared.GameModules;

using Octaryn.Shared.Host;

public readonly record struct ModuleHostContext(
    IHostCommandSink Commands);
