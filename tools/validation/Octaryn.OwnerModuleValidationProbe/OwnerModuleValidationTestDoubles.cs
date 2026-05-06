using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

internal sealed class TestRegistration(GameModuleManifest manifest) : IGameModuleRegistration
{
    public GameModuleManifest Manifest { get; } = manifest;

    public IGameModuleInstance CreateInstance(ModuleHostContext context)
    {
        _ = context;
        return new TestInstance();
    }
}

internal sealed class ThrowingCreateRegistration(
    GameModuleManifest manifest,
    Exception exception) : IGameModuleRegistration
{
    public GameModuleManifest Manifest { get; } = manifest;

    public IGameModuleInstance CreateInstance(ModuleHostContext context)
    {
        _ = context;
        throw exception;
    }
}

internal sealed class ThrowingDisposeRegistration(
    GameModuleManifest manifest,
    Exception exception) : IGameModuleRegistration
{
    public GameModuleManifest Manifest { get; } = manifest;

    public IGameModuleInstance CreateInstance(ModuleHostContext context)
    {
        _ = context;
        return new ThrowingDisposeInstance(exception);
    }
}

internal sealed class TestInstance : IGameModuleInstance
{
    public void Tick(in ModuleFrameContext frame)
    {
        _ = frame;
    }

    public void Dispose()
    {
    }
}

internal sealed class ThrowingDisposeInstance(Exception exception) : IGameModuleInstance
{
    public void Tick(in ModuleFrameContext frame)
    {
        _ = frame;
    }

    public void Dispose()
    {
        throw exception;
    }
}

internal sealed class TestCommandSink : IHostCommandSink
{
    private HostCommand _lastCommand;

    public bool Enqueue(HostCommand command)
    {
        _lastCommand = command;
        return command.Version == HostCommand.VersionValue &&
            command.Size == HostCommand.SizeValue;
    }

    public void ExpectLastBlockEdit(BlockPosition position, BlockId block, ulong requestId)
    {
        if (_lastCommand.Kind != HostCommandKind.SetBlock ||
            _lastCommand.Flags != HostCommand.CriticalFlag ||
            _lastCommand.RequestId != requestId ||
            _lastCommand.A != position.X ||
            _lastCommand.B != position.Y ||
            _lastCommand.C != position.Z ||
            _lastCommand.D != block.Value)
        {
            throw new InvalidOperationException("host context did not map block edit command payload.");
        }
    }
}
