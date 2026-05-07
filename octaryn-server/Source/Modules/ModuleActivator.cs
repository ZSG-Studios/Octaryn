using Octaryn.Server.Modules.Bundled;
using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Persistence.Players;
using Octaryn.Server.Simulation.Players;
using Octaryn.Server.Tick;
using Octaryn.Server.Validation;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Chunks;
using Octaryn.Server.World.Generation;
using Octaryn.Server.World.Time;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;
using Octaryn.Shared.Time;
using Octaryn.Shared.World;

namespace Octaryn.Server.Modules;

internal sealed class ModuleActivator : IDisposable
{
    private readonly IGameModuleRegistration? _registration;
    private readonly bool _requiresBundledMetadata;
    private readonly WorldTimeClock _worldTime = new();
    private readonly BlockStore _blocks = new();
    private readonly BlockEditService _blockEdits;
    private readonly BlockChangeQueue _blockChanges = new();
    private readonly WorldBlockPersistence _blockPersistence;
    private readonly PlayerController _playerController;
    private readonly BlockCommandSink _blockCommands;
    private readonly ClientBlockCommandQueue _clientBlockCommands;
    private readonly NativeScheduleRuntime _scheduleRuntime = new();
    private readonly AuthorityTickRunner _authorityTick;
    private readonly ChunkColumnStreamProvider _chunkColumns;
    private ulong _lastTickId;
    private IGameModuleInstance? _instance;
    private bool _modulelessActive;
    private bool _isDisposed;

    public ModuleActivator()
        : this(Loader.LoadBundledRegistration(), requiresBundledMetadata: true)
    {
    }

    public ModuleActivator(IGameModuleRegistration registration)
        : this(registration, requiresBundledMetadata: false)
    {
    }

    public static ModuleActivator CreateWithoutGameModules()
    {
        return new ModuleActivator(registration: null, requiresBundledMetadata: false);
    }

    private ModuleActivator(IGameModuleRegistration? registration, bool requiresBundledMetadata)
    {
        _registration = registration;
        _requiresBundledMetadata = requiresBundledMetadata;
        var blockAuthorityRules = registration is IBlockAuthorityRulesProvider authorityRulesProvider
            ? authorityRulesProvider.BlockAuthorityRules
            : registration is null
                ? NativeEmptyWorldBlockAuthorityRules.Instance
                : DenyBlockAuthorityRules.Instance;
        Func<BlockPosition, BlockId>? generatedBlockProvider = null;
        var hasGeneratedTerrain = false;
        var hasNativeEmptyWorld = false;
        var clearedGeneratedOverrides = 0;
        var terrainRules = default(NativeTerrainMaterialRules);
        if (registration is IWorldGenerationRulesProvider worldGenerationRulesProvider)
        {
            terrainRules = NativeTerrainGenerationLibrary.MaterialRulesFrom(worldGenerationRulesProvider.WorldGenerationRules);
            generatedBlockProvider = position => NativeTerrainGenerationLibrary.GeneratedBlock(position, in terrainRules);
            hasGeneratedTerrain = true;
        }
        else if (registration is null)
        {
            generatedBlockProvider = NativeTerrainGenerationLibrary.EmptyWorldGeneratedBlock;
            hasNativeEmptyWorld = true;
        }

        _blockPersistence = WorldBlockPersistence.FromEnvironment();
        _blockPersistence.Load(_blocks);
        if (hasGeneratedTerrain)
        {
            clearedGeneratedOverrides = NativeTerrainGenerationLibrary.ClearTerrainMatchingOverrides(_blocks, in terrainRules);
        }
        else if (hasNativeEmptyWorld)
        {
            clearedGeneratedOverrides = NativeTerrainGenerationLibrary.ClearEmptyWorldMatchingOverrides(_blocks);
        }

        if (clearedGeneratedOverrides != 0)
        {
            _blockPersistence.MarkDirty();
            LiveDebugLog.Write($"server_live_world_override_cleanup generated_matches={clearedGeneratedOverrides} blocks={_blocks.BlockCount}");
        }
        _chunkColumns = new ChunkColumnStreamProvider(_blocks, generatedBlockProvider is not null);

        _playerController = new PlayerController(
            PlayerPersistence.FromEnvironment(),
            _blocks,
            blockAuthorityRules,
            generatedBlockProvider);
        LiveDebugLog.Write($"server_live_world_loaded blocks={_blocks.BlockCount}");
        _blockEdits = new BlockEditService(
            _blocks,
            blockAuthorityRules,
            generatedBlockProvider);
        _blockCommands = new BlockCommandSink(_blockEdits, _blockChanges, MarkBlockPersistenceDirty);
        _clientBlockCommands = new ClientBlockCommandQueue(_blockCommands, blockAuthorityRules);
        _authorityTick = new AuthorityTickRunner(_scheduleRuntime, _playerController, _worldTime);

        LiveDebugLog.Write($"server_live_world_generation available={(hasGeneratedTerrain ? 1 : 0)} native_empty={(hasNativeEmptyWorld ? 1 : 0)}");
    }

    public bool IsActive => _instance is not null || _modulelessActive;

    internal WorldTimeSnapshot SnapshotWorldTime()
    {
        return _worldTime.Snapshot();
    }

    internal PlayerState SnapshotPlayer()
    {
        return _playerController.Snapshot();
    }

    internal void SetWorldTimeSpeedMultiplier(double multiplier)
    {
        _worldTime.SetSpeedMultiplier(multiplier);
    }

    internal BlockId GetBlock(BlockPosition position)
    {
        return _blockEdits.GetBlock(position);
    }

    internal IReadOnlyList<BlockEdit> SnapshotBlocks()
    {
        return _blocks.Snapshot();
    }

    internal unsafe int RequestChunkColumns(ChunkColumnRequestFrame* requestFrame)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        return _chunkColumns.RequestChunkColumns(requestFrame);
    }

    internal NativeChunkStreamSnapshotResult WriteChunkStreamSnapshotFile(
        string streamPath,
        ulong epoch,
        int centerChunkX,
        int centerChunkZ,
        uint radius,
        bool hasPreviousWindow,
        int previousCenterChunkX,
        int previousCenterChunkZ,
        uint previousRadius,
        bool metadataOnly,
        WorldTimeSnapshot worldTime,
        PlayerState playerState)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        return _chunkColumns.WriteSnapshotFile(
            streamPath,
            epoch,
            centerChunkX,
            centerChunkZ,
            radius,
            hasPreviousWindow,
            previousCenterChunkX,
            previousCenterChunkZ,
            previousRadius,
            metadataOnly,
            worldTime,
            playerState);
    }

    internal int WorldBlockCount => _blocks.BlockCount;

    internal int PendingClientBlockCommandCount => _clientBlockCommands.PendingCount;

    internal int PendingBlockChangeCount => _blockChanges.PendingCount;

    public int Activate(IHostCommandSink commandSink)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);

        if (_instance is not null)
        {
            return 0;
        }

        if (_registration is null)
        {
            _modulelessActive = true;
            _playerController.AlignSpawnToSurface();
            _blockPersistence.EnsureInitialized(_blocks);
            LiveDebugLog.Write($"server_live_activate active=1 module=none blocks={_blocks.BlockCount} pending_block_changes={_blockChanges.PendingCount}");
            return 0;
        }

        var validationReport = ModuleValidation.Validate(_registration);
        if (!validationReport.IsValid)
        {
            LiveDebugLog.Write("server_live_module_validation valid=0");
            return -2;
        }
        LiveDebugLog.Write("server_live_module_validation valid=1");

        var bundledManifest = Catalog.ResolveManifest(_registration.Manifest.ModuleId);
        if ((bundledManifest is null && _requiresBundledMetadata) ||
            (bundledManifest is not null && !BundledModuleMetadataVerifier.Matches(bundledManifest, _registration.Manifest)))
        {
            LiveDebugLog.Write($"server_live_bundled_module valid=0 module={_registration.Manifest.ModuleId}");
            return -3;
        }
        LiveDebugLog.Write($"server_live_bundled_module valid=1 module={_registration.Manifest.ModuleId}");

        try
        {
            var serverCommandSink = new BlockCommandSink(_blockEdits, _blockChanges, MarkBlockPersistenceDirty, commandSink);
            _instance = _registration.CreateInstance(HostModuleContext.Create(_registration.Manifest, serverCommandSink));
            _playerController.AlignSpawnToSurface();
            _blockPersistence.EnsureInitialized(_blocks);
            LiveDebugLog.Write($"server_live_activate active=1 blocks={_blocks.BlockCount} pending_block_changes={_blockChanges.PendingCount}");
        }
        catch
        {
            _instance?.Dispose();
            _instance = null;
            throw;
        }

        return 0;
    }

    public void Tick(in HostFrameSnapshot snapshot)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        if (_registration is null)
        {
            TickHostOnly(in snapshot);
            return;
        }

        if (_instance is null)
        {
            return;
        }

        var pendingClientCommands = _clientBlockCommands.PendingCount;
        var frame = HostFrameContext.FromSnapshot(in snapshot);
        var worldTime = _authorityTick.Execute(
            in frame,
            _clientBlockCommands.Drain,
            out var appliedClientCommands);
        _lastTickId = worldTime.TickId;
        var moduleFrame = new ModuleFrameContext(frame.DeltaSeconds, frame.FrameIndex, worldTime);
        _scheduleRuntime.ExecuteCommandWriteMainThread(
            "server.module.tick",
            () => _instance.Tick(in moduleFrame));

        _blockPersistence.SaveIfDirty(_blocks);
        LiveDebugLog.Write($"server_live_tick frame={frame.FrameIndex} tick={_lastTickId} dt={frame.DeltaSeconds:F6} client_commands_pending_before={pendingClientCommands} client_commands_applied={appliedClientCommands} blocks={_blocks.BlockCount} pending_block_changes={_blockChanges.PendingCount}");
    }

    internal void TickHostOnly(in HostFrameSnapshot snapshot)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        var pendingClientCommands = _clientBlockCommands.PendingCount;
        var frame = HostFrameContext.FromSnapshot(in snapshot);
        var worldTime = _authorityTick.Execute(
            in frame,
            _clientBlockCommands.Drain,
            out var appliedClientCommands);
        _lastTickId = worldTime.TickId;
        _blockPersistence.SaveIfDirty(_blocks);
        LiveDebugLog.Write($"server_live_tick frame={frame.FrameIndex} tick={_lastTickId} dt={frame.DeltaSeconds:F6} host_only=1 module={(_registration is null ? "none" : _registration.Manifest.ModuleId)} client_commands_pending_before={pendingClientCommands} client_commands_applied={appliedClientCommands} blocks={_blocks.BlockCount} pending_block_changes={_blockChanges.PendingCount}");
    }

    internal unsafe int SubmitClientCommands(HostCommand* commands, uint commandCount)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        var report = _clientBlockCommands.Submit(commands, commandCount);
        LiveDebugLog.Write($"server_live_client_commands_submit requested={report.RequestedCount} pending_before={report.PendingBefore}");
        if (report.Reason == NativeClientBlockCommandSubmitReason.Capacity)
        {
            LiveDebugLog.Write($"server_live_client_commands_submit result={report.Result} reason={NativeBlockStoreLibrary.ClientBlockCommandSubmitReasonLabel(report.Reason)} requested={report.RequestedCount}");
            return report.Result;
        }

        if (report.Reason == NativeClientBlockCommandSubmitReason.RejectedCommand)
        {
            var rejectedIndex = checked((int)report.RejectedIndex);
            var rejectedCommand = commands[rejectedIndex];
            LiveDebugLog.Write($"server_live_client_command_rejected index={report.RejectedIndex} kind={rejectedCommand.Kind} request={rejectedCommand.RequestId} edit={NativeBlockStoreLibrary.HostCommandEditLabel(rejectedCommand)} block=({rejectedCommand.A},{rejectedCommand.B},{rejectedCommand.C},{rejectedCommand.D})");
            return report.Result;
        }

        if (report.Result != 0)
        {
            LiveDebugLog.Write($"server_live_client_commands_submit result={report.Result} reason={NativeBlockStoreLibrary.ClientBlockCommandSubmitReasonLabel(report.Reason)}");
            return report.Result;
        }

        var requestedCount = checked((int)report.RequestedCount);
        for (var index = 0; index < requestedCount; index++)
        {
            LiveDebugLog.Write($"server_live_client_command_queued index={index} kind={commands[index].Kind} request={commands[index].RequestId} edit={NativeBlockStoreLibrary.HostCommandEditLabel(commands[index])} block=({commands[index].A},{commands[index].B},{commands[index].C},{commands[index].D})");
        }

        LiveDebugLog.Write($"server_live_client_commands_submit result=0 pending_after={report.PendingAfter}");
        return 0;
    }

    internal unsafe int SubmitClientCommands(IReadOnlyList<HostCommand> commands)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        if (commands.Count == 0)
        {
            return SubmitClientCommands(null, 0);
        }

        var commandBuffer = commands as HostCommand[] ?? commands.ToArray();
        fixed (HostCommand* commandPointer = commandBuffer)
        {
            return SubmitClientCommands(commandPointer, (uint)commandBuffer.Length);
        }
    }

    internal unsafe int DrainServerSnapshots(ServerSnapshotHeader* snapshotHeader)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        var report = _blockChanges.DrainSnapshotReport(snapshotHeader, _lastTickId);
        if (report.Result != 0)
        {
            LiveDebugLog.Write($"server_live_snapshot_drain result={report.Result} tick={_lastTickId} requested_capacity={report.RequestedCapacity} pending_before={report.PendingBefore}");
            return report.Result;
        }

        LiveDebugLog.Write($"server_live_snapshot_drain result=0 tick={_lastTickId} requested_capacity={report.RequestedCapacity} pending_before={report.PendingBefore} written={report.Written}");
        return 0;
    }

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        try
        {
            _instance?.Dispose();
        }
        finally
        {
            _playerController.Dispose();
            _blockPersistence.SaveIfDirty(_blocks);
            _clientBlockCommands.Dispose();
            _scheduleRuntime.Dispose();
            _blockPersistence.Dispose();
            _worldTime.Dispose();
            _blockChanges.Dispose();
            _blocks.Dispose();
            _instance = null;
        }
    }

    private void MarkBlockPersistenceDirty(int editCount)
    {
        LiveDebugLog.Write($"server_live_block_persistence_dirty edits={editCount}");
        _blockPersistence.MarkDirty();
    }

}
