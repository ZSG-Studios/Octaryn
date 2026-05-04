using Octaryn.Server.Persistence.WorldBlocks;
using Octaryn.Server.Tick;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Chunks;
using Octaryn.Server.World.Generation;
using Octaryn.Server.World.Time;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;
using Octaryn.Shared.World;

namespace Octaryn.Server;

internal sealed class ServerModuleActivator : IDisposable
{
    private readonly IGameModuleRegistration _registration;
    private readonly bool _requiresBundledMetadata;
    private readonly WorldTimeClock _worldTime = new();
    private readonly ServerBlockStore _blocks = new();
    private readonly ServerBlockEditService _blockEdits;
    private readonly ServerBlockChangeQueue _blockChanges = new();
    private readonly ServerWorldBlockPersistence _blockPersistence;
    private readonly ServerBlockCommandSink _blockCommands;
    private readonly ServerClientBlockCommandQueue _clientBlockCommands;
    private readonly ServerTerrainGenerator? _terrainGenerator;
    private ulong _lastTickId;
    private ServerHostScheduler? _scheduler;
    private IGameModuleInstance? _instance;
    private bool _isDisposed;

    public ServerModuleActivator()
        : this(ServerBundledModuleLoader.LoadBasegameRegistration(), requiresBundledMetadata: true)
    {
    }

    public ServerModuleActivator(IGameModuleRegistration registration)
        : this(registration, requiresBundledMetadata: false)
    {
    }

    private ServerModuleActivator(IGameModuleRegistration registration, bool requiresBundledMetadata)
    {
        _registration = registration;
        _requiresBundledMetadata = requiresBundledMetadata;
        var blockAuthorityRules = registration is IBlockAuthorityRulesProvider authorityRulesProvider
            ? authorityRulesProvider.BlockAuthorityRules
            : ServerDenyBlockAuthorityRules.Instance;
        _blockPersistence = ServerWorldBlockPersistence.FromEnvironment();
        _blockPersistence.Load(_blocks);
        ServerLiveDebugLog.Write($"server_live_world_loaded blocks={_blocks.BlockCount}");
        _blockEdits = new ServerBlockEditService(_blocks, blockAuthorityRules);
        _blockCommands = new ServerBlockCommandSink(_blockEdits, _blockChanges, MarkBlockPersistenceDirty);
        _clientBlockCommands = new ServerClientBlockCommandQueue(_blockCommands, blockAuthorityRules);
        if (registration is IWorldGenerationRulesProvider worldGenerationRulesProvider)
        {
            _terrainGenerator = new ServerTerrainGenerator(worldGenerationRulesProvider.WorldGenerationRules);
        }

        ServerLiveDebugLog.Write($"server_live_world_generation available={(_terrainGenerator is null ? 0 : 1)}");
    }

    public bool IsActive => _instance is not null;

    internal BlockId GetBlock(BlockPosition position)
    {
        return _blockEdits.GetBlock(position);
    }

    internal IReadOnlyList<BlockEdit> SnapshotBlocks()
    {
        return _blocks.Snapshot();
    }

    internal IReadOnlyList<BlockEdit> GenerateTerrainChunkColumn(int originX, int originZ)
    {
        var edits = _terrainGenerator?.GenerateChunkColumn(originX, originZ) ?? [];
        ServerLiveDebugLog.Write($"server_live_chunk_generate origin=({originX},{originZ}) edits={edits.Count}");
        return edits;
    }

    internal unsafe int RequestChunkColumns(ChunkColumnRequestFrame* requestFrame)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        if (requestFrame is null ||
            requestFrame->Version != ChunkColumnRequestFrame.VersionValue ||
            requestFrame->Size != ChunkColumnRequestFrame.SizeValue)
        {
            return -1;
        }

        if (_terrainGenerator is null)
        {
            return WriteChunkColumnRequestResult(requestFrame, 0, 0, status: 5);
        }

        if (requestFrame->Radius > ChunkColumnStreamingLimits.MaxRequestRadius)
        {
            return WriteChunkColumnRequestResult(requestFrame, 0, 0, status: 2);
        }

        var columnCount = CheckedColumnCount(requestFrame->Radius);
        if (requestFrame->ColumnCapacity < columnCount)
        {
            return WriteChunkColumnRequestResult(requestFrame, columnCount, 0, status: 3);
        }

        if (requestFrame->ColumnsAddress == 0 ||
            (requestFrame->BlockCapacity > 0 && requestFrame->BlocksAddress == 0))
        {
            return -1;
        }

        var stream = CaptureChunkColumns(
            requestFrame->CenterChunkX,
            requestFrame->CenterChunkZ,
            requestFrame->Radius,
            windowEpoch: 0);
        if (requestFrame->BlockCapacity < stream.Blocks.Count)
        {
            return WriteChunkColumnRequestResult(requestFrame, columnCount, (uint)stream.Blocks.Count, status: 4);
        }

        var columns = (ChunkColumnSnapshotColumn*)requestFrame->ColumnsAddress;
        for (var index = 0; index < stream.Columns.Count; index++)
        {
            var column = stream.Columns[index];
            columns[index] = new ChunkColumnSnapshotColumn(
                column.ChunkX,
                column.ChunkZ,
                column.OriginX,
                column.OriginZ,
                column.BlockOffset,
                column.BlockCount);
        }

        var blocks = (ChunkColumnSnapshotBlock*)requestFrame->BlocksAddress;
        for (var index = 0; index < stream.Blocks.Count; index++)
        {
            var block = stream.Blocks[index];
            blocks[index] = new ChunkColumnSnapshotBlock(
                block.X,
                block.Y,
                block.Z,
                block.Block);
        }

        ServerLiveDebugLog.Write($"server_live_chunk_request center=({requestFrame->CenterChunkX},{requestFrame->CenterChunkZ}) radius={requestFrame->Radius} columns={stream.Columns.Count} blocks={stream.Blocks.Count}");
        return WriteChunkColumnRequestResult(requestFrame, (uint)stream.Columns.Count, (uint)stream.Blocks.Count, status: 0);
    }

    internal ServerChunkColumnStream CaptureChunkColumns(
        int centerChunkX,
        int centerChunkZ,
        uint radius,
        ulong windowEpoch,
        bool hasPreviousWindow = false,
        int previousCenterChunkX = 0,
        int previousCenterChunkZ = 0,
        uint previousRadius = 0)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        if (radius > ChunkColumnStreamingLimits.MaxRequestRadius)
        {
            throw new ArgumentOutOfRangeException(nameof(radius));
        }

        var window = ServerChunkWindow.Plan(new ServerChunkWindowIntent(
            windowEpoch,
            centerChunkX,
            centerChunkZ,
            radius,
            hasPreviousWindow,
            previousCenterChunkX,
            previousCenterChunkZ,
            previousRadius));

        if (_terrainGenerator is null)
        {
            return new ServerChunkColumnStream(centerChunkX, centerChunkZ, radius, window, [], []);
        }

        List<ServerChunkColumnStreamColumn> columns = [];
        List<ServerChunkColumnStreamBlock> blocks = [];
        var radiusInt = (int)radius;
        for (var chunkZ = centerChunkZ - radiusInt; chunkZ <= centerChunkZ + radiusInt; chunkZ++)
        for (var chunkX = centerChunkX - radiusInt; chunkX <= centerChunkX + radiusInt; chunkX++)
        {
            var originX = checked(chunkX * ChunkConstants.Width);
            var originZ = checked(chunkZ * ChunkConstants.Depth);
            var edits = ChunkColumnBlocks(originX, originZ);
            var blockOffset = (uint)blocks.Count;
            foreach (var edit in edits)
            {
                blocks.Add(new ServerChunkColumnStreamBlock(
                    edit.Position.X,
                    edit.Position.Y,
                    edit.Position.Z,
                    edit.Block.Value));
            }

            columns.Add(new ServerChunkColumnStreamColumn(
                chunkX,
                chunkZ,
                originX,
                originZ,
                blockOffset,
                (uint)edits.Count));
        }

        return new ServerChunkColumnStream(centerChunkX, centerChunkZ, radius, window, columns, blocks);
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

        var validationReport = ServerModuleValidation.Validate(_registration);
        if (!validationReport.IsValid)
        {
            ServerLiveDebugLog.Write("server_live_module_validation valid=0");
            return -2;
        }
        ServerLiveDebugLog.Write("server_live_module_validation valid=1");

        var bundledManifest = ServerBundledModuleCatalog.ResolveManifest(_registration.Manifest.ModuleId);
        if ((bundledManifest is null && _requiresBundledMetadata) ||
            (bundledManifest is not null && !BundledModuleMetadataVerifier.Matches(bundledManifest, _registration.Manifest)))
        {
            ServerLiveDebugLog.Write($"server_live_bundled_module valid=0 module={_registration.Manifest.ModuleId}");
            return -3;
        }
        ServerLiveDebugLog.Write($"server_live_bundled_module valid=1 module={_registration.Manifest.ModuleId}");

        var scheduler = new ServerHostScheduler(_registration.Manifest.Schedule.Systems);
        try
        {
            var serverCommandSink = new ServerBlockCommandSink(_blockEdits, _blockChanges, MarkBlockPersistenceDirty, commandSink);
            _instance = _registration.CreateInstance(HostModuleContext.Create(_registration.Manifest, serverCommandSink));
            _scheduler = scheduler;
            SeedInitialWorldIfNeeded();
            _blockPersistence.EnsureInitialized(_blocks);
            ServerLiveDebugLog.Write($"server_live_activate active=1 blocks={_blocks.BlockCount} pending_block_changes={_blockChanges.PendingCount}");
        }
        catch
        {
            _instance?.Dispose();
            _instance = null;
            _scheduler = null;
            scheduler.Dispose();
            throw;
        }

        return 0;
    }

    public void Tick(in HostFrameSnapshot snapshot)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        if (_instance is null || _scheduler is null)
        {
            return;
        }

        var pendingClientCommands = _clientBlockCommands.PendingCount;
        var appliedClientCommands = _clientBlockCommands.Drain();

        var frame = HostFrameContext.FromSnapshot(in snapshot);
        var worldTime = _worldTime.AdvanceFrame(frame.DeltaSeconds);
        _lastTickId = worldTime.TickId;
        var moduleFrame = new ModuleFrameContext(frame.DeltaSeconds, frame.FrameIndex, worldTime);
        var declaration = _registration.Manifest.Schedule.Systems[0];
        var work = HostScheduledWork.FromDeclaration(
            declaration,
            _ => _instance.Tick(in moduleFrame));
        if (!_scheduler.TryRun(work, frame))
        {
            throw new InvalidOperationException("Server module tick could not be scheduled by the host.");
        }

        _blockPersistence.SaveIfDirty(_blocks);
        ServerLiveDebugLog.Write($"server_live_tick frame={frame.FrameIndex} tick={_lastTickId} dt={frame.DeltaSeconds:F6} client_commands_pending_before={pendingClientCommands} client_commands_applied={appliedClientCommands} blocks={_blocks.BlockCount} pending_block_changes={_blockChanges.PendingCount}");
    }

    internal unsafe int SubmitClientCommands(HostCommand* commands, uint commandCount)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        if (commandCount > ServerClientBlockCommandQueue.MaxPendingCommands ||
            (commandCount > 0 && commands is null))
        {
            return -1;
        }

        var requestedCount = (int)commandCount;
        ServerLiveDebugLog.Write($"server_live_client_commands_submit requested={requestedCount} pending_before={_clientBlockCommands.PendingCount}");
        if (_clientBlockCommands.PendingCount > ServerClientBlockCommandQueue.MaxPendingCommands - requestedCount)
        {
            ServerLiveDebugLog.Write($"server_live_client_commands_submit result=-1 reason=capacity requested={requestedCount}");
            return -1;
        }

        for (var index = 0; index < requestedCount; index++)
        {
            if (!_clientBlockCommands.CanQueue(commands[index]))
            {
                ServerLiveDebugLog.Write($"server_live_client_command_rejected index={index} kind={commands[index].Kind} request={commands[index].RequestId} edit={ServerBlockCommandDiagnostics.EditLabel(commands[index])} block=({commands[index].A},{commands[index].B},{commands[index].C},{commands[index].D})");
                return -2;
            }
        }

        for (var index = 0; index < requestedCount; index++)
        {
            if (!_clientBlockCommands.Enqueue(commands[index]))
            {
                ServerLiveDebugLog.Write($"server_live_client_command_rejected index={index} kind={commands[index].Kind} request={commands[index].RequestId} edit={ServerBlockCommandDiagnostics.EditLabel(commands[index])} block=({commands[index].A},{commands[index].B},{commands[index].C},{commands[index].D})");
                return -2;
            }
            ServerLiveDebugLog.Write($"server_live_client_command_queued index={index} kind={commands[index].Kind} request={commands[index].RequestId} edit={ServerBlockCommandDiagnostics.EditLabel(commands[index])} block=({commands[index].A},{commands[index].B},{commands[index].C},{commands[index].D})");
        }

        ServerLiveDebugLog.Write($"server_live_client_commands_submit result=0 pending_after={_clientBlockCommands.PendingCount}");
        return 0;
    }

    internal unsafe int DrainServerSnapshots(ServerSnapshotHeader* snapshotHeader)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        if (snapshotHeader is null ||
            snapshotHeader->Version != ServerSnapshotHeader.VersionValue ||
            snapshotHeader->Size != ServerSnapshotHeader.SizeValue)
        {
            return -1;
        }

        var changeCapacity = snapshotHeader->ChangeCount;
        var changes = (ReplicationChange*)snapshotHeader->ChangesAddress;
        var pendingBefore = _blockChanges.PendingCount;
        var result = _blockChanges.Drain(changes, changeCapacity, _lastTickId, out var changeCount);
        if (result != 0)
        {
            ServerLiveDebugLog.Write($"server_live_snapshot_drain result={result} tick={_lastTickId} requested_capacity={changeCapacity} pending_before={pendingBefore}");
            return result;
        }

        *snapshotHeader = new ServerSnapshotHeader(
            replicationCount: 0,
            changeCount,
            tickId: _lastTickId,
            replicationIdsAddress: snapshotHeader->ReplicationIdsAddress,
            changesAddress: snapshotHeader->ChangesAddress);
        ServerLiveDebugLog.Write($"server_live_snapshot_drain result=0 tick={_lastTickId} requested_capacity={changeCapacity} pending_before={pendingBefore} written={changeCount}");
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
            _blockPersistence.SaveIfDirty(_blocks);
            _scheduler?.Dispose();
            _instance = null;
            _scheduler = null;
        }
    }

    private void MarkBlockPersistenceDirty(IReadOnlyList<BlockEdit> edits)
    {
        ServerLiveDebugLog.Write($"server_live_block_persistence_dirty edits={edits.Count}");
        _blockPersistence.MarkDirty();
    }

    private static uint CheckedColumnCount(uint radius)
    {
        var width = checked(radius * 2u + 1u);
        return checked(width * width);
    }

    private IReadOnlyList<BlockEdit> ChunkColumnBlocks(int originX, int originZ)
    {
        var loadedBlocks = _blocks.SnapshotChunkColumn(originX, originZ);
        if (loadedBlocks.Count != 0)
        {
            return loadedBlocks;
        }

        return _terrainGenerator?.GenerateChunkColumn(originX, originZ) ?? [];
    }

    private static unsafe int WriteChunkColumnRequestResult(
        ChunkColumnRequestFrame* requestFrame,
        uint columnCount,
        uint blockCount,
        uint status)
    {
        *requestFrame = new ChunkColumnRequestFrame(
            requestFrame->CenterChunkX,
            requestFrame->CenterChunkZ,
            requestFrame->Radius,
            requestFrame->ColumnCapacity,
            requestFrame->BlockCapacity,
            columnCount,
            blockCount,
            status,
            requestFrame->ColumnsAddress,
            requestFrame->BlocksAddress);
        return status == 0 ? 0 : -(int)status;
    }

    private void SeedInitialWorldIfNeeded()
    {
        if (_terrainGenerator is null || !ServerInitialWorldSeeder.ShouldSeedSpawnChunkColumn(_blocks))
        {
            ServerLiveDebugLog.Write($"server_live_seed_spawn skipped=1 blocks={_blocks.BlockCount} terrain_generator={(_terrainGenerator is null ? 0 : 1)}");
            return;
        }

        var seeded = ServerInitialWorldSeeder.SeedSpawnChunkColumn(_terrainGenerator, _blocks);
        ServerLiveDebugLog.Write($"server_live_seed_spawn skipped=0 origin=({ServerInitialWorldSeeder.SpawnChunkOriginX},{ServerInitialWorldSeeder.SpawnChunkOriginZ}) edits={seeded} blocks={_blocks.BlockCount}");
        if (seeded > 0)
        {
            _blockPersistence.MarkDirty();
        }
    }
}
