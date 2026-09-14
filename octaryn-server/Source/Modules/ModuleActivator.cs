using Octaryn.Server.Modules.Bundled;
using Octaryn.Server.Persistence.WorldBlocks;
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
    private readonly IBlockAuthorityRules _itemBlockRules;
    private readonly FluidSimulation? _fluids;
    private readonly BlockChangeQueue? _blockChanges;
    private readonly WorldBlockPersistence _blockPersistence;
    private readonly PlayerController _playerController;
    private readonly ClientBlockCommandQueue _clientBlockCommands;
    private readonly NativeScheduleRuntime _scheduleRuntime = new();
    private readonly AuthorityTickRunner _authorityTick;
    private readonly ChunkColumnStreamProvider _chunkColumns;
    private ulong _lastTickId;
    private IGameModuleInstance? _instance;
    private bool _modulelessActive;
    private bool _isDisposed;

    public ModuleActivator(BlockPublicationMode publicationMode = BlockPublicationMode.ReplicationDeltas)
        : this(Loader.LoadBundledRegistration(), requiresBundledMetadata: true, publicationMode)
    {
    }

    public ModuleActivator(IGameModuleRegistration registration,
        BlockPublicationMode publicationMode = BlockPublicationMode.ReplicationDeltas)
        : this(registration, requiresBundledMetadata: false, publicationMode)
    {
    }

    public static ModuleActivator CreateWithoutGameModules(
        BlockPublicationMode publicationMode = BlockPublicationMode.ReplicationDeltas)
    {
        return new ModuleActivator(registration: null, requiresBundledMetadata: false, publicationMode);
    }

    private ModuleActivator(IGameModuleRegistration? registration, bool requiresBundledMetadata,
        BlockPublicationMode publicationMode)
    {
        if (publicationMode is not (BlockPublicationMode.ReplicationDeltas or BlockPublicationMode.ProcessSnapshots))
            throw new ArgumentOutOfRangeException(nameof(publicationMode));
        PublicationMode = publicationMode;
        _blockChanges = publicationMode == BlockPublicationMode.ReplicationDeltas ? new BlockChangeQueue() : null;
        _registration = registration;
        _requiresBundledMetadata = requiresBundledMetadata;
        var blockAuthorityRules = registration is IBlockAuthorityRulesProvider authorityRulesProvider
            ? authorityRulesProvider.BlockAuthorityRules
            : registration is null
                ? NativeEmptyWorldBlockAuthorityRules.Instance
                : DenyBlockAuthorityRules.Instance;
        Func<BlockPosition, BlockId>? generatedBlockProvider = null;
        _itemBlockRules = blockAuthorityRules;
        var hasGeneratedTerrain = false;
        var hasNativeEmptyWorld = false;
        var clearedGeneratedOverrides = 0;
        var terrainRules = default(NativeTerrainMaterialRules);
        var useFlatTestTerrain = IsFlatTestTerrainEnabled();
        if (registration is IWorldGenerationRulesProvider worldGenerationRulesProvider)
        {
            terrainRules = NativeTerrainGenerationLibrary.MaterialRulesFrom(worldGenerationRulesProvider.WorldGenerationRules);
            generatedBlockProvider = useFlatTestTerrain
                ? position => NativeTerrainGenerationLibrary.FlatTestGeneratedBlock(position, in terrainRules)
                : position => NativeTerrainGenerationLibrary.GeneratedBlock(position, in terrainRules);
            hasGeneratedTerrain = true;
        }
        else if (registration is null)
        {
            generatedBlockProvider = NativeTerrainGenerationLibrary.EmptyWorldGeneratedBlock;
            hasNativeEmptyWorld = true;
        }

        var generationMode = hasGeneratedTerrain ? (useFlatTestTerrain ? 1u : 0u) : 2u;
        NativeWorldPersistenceLibrary.EnsureWorldGeneration(generationMode);
        _blockPersistence = WorldBlockPersistence.FromEnvironment();
        _blockPersistence.Load(_blocks);
        if (hasGeneratedTerrain && useFlatTestTerrain)
        {
            clearedGeneratedOverrides = NativeTerrainGenerationLibrary.ClearFlatTestMatchingOverrides(_blocks, in terrainRules);
        }
        else if (hasGeneratedTerrain)
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
        _chunkColumns = new ChunkColumnStreamProvider(_blocks, generatedBlockProvider is not null, generationMode);

        _playerController = new PlayerController(
            NativeWorldPersistenceLibrary.PlayerDirectoryPathFromEnvironment(),
            _blocks,
            blockAuthorityRules,
            generatedBlockProvider);
        LiveDebugLog.Write($"server_live_world_loaded blocks={_blocks.BlockCount}");
        _blockEdits = new BlockEditService(
            _blocks,
            blockAuthorityRules,
            generatedBlockProvider);
        _fluids = registration is IFluidRulesProvider fluidRulesProvider
            ? new FluidSimulation(fluidRulesProvider.FluidRules, blockAuthorityRules) : null;
        _clientBlockCommands = new ClientBlockCommandQueue(
            _blockEdits,
            blockAuthorityRules,
            _blockChanges,
            OnBlocksChanged,
            command => !_playerController.PlacementIntersectsPlayer(command));
        _authorityTick = new AuthorityTickRunner(_scheduleRuntime, _playerController, _worldTime);

        LiveDebugLog.Write($"server_live_world_generation available={(hasGeneratedTerrain ? 1 : 0)} native_empty={(hasNativeEmptyWorld ? 1 : 0)} flat_test={(useFlatTestTerrain ? 1 : 0)}");
    }

    private static bool IsFlatTestTerrainEnabled()
    {
        var value = Environment.GetEnvironmentVariable("OCTARYN_SERVER_FLAT_TEST_TERRAIN");
        return !string.IsNullOrWhiteSpace(value) && value != "0";
    }

    public bool IsActive => _instance is not null || _modulelessActive;

    internal ulong BlockRevision { get; private set; }

    internal BlockPublicationMode PublicationMode { get; }

    internal const int DeltaSnapshotsUnsupported = -2;

    internal ChunkPublicationTracker ChunkPublication { get; } = new();

    internal NativeFluidTickReport FluidReport { get; private set; }
    internal double FluidStepMilliseconds { get; private set; }

    internal void SetFluidRegion(int centerChunkX, int centerChunkZ, uint radius)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        _fluids?.SetRegion(centerChunkX, centerChunkZ, radius);
    }

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

    internal bool IsItemBlockPlaceable(ushort block) =>
        block != 0 && _itemBlockRules.IsKnownBlock(new BlockId(block)) &&
        _itemBlockRules.IsClientPlaceable(new BlockId(block));

    internal bool IsItemCollisionSolid(BlockPosition position) =>
        _itemBlockRules.IsSolidBlock(_blockEdits.GetBlock(position));

    internal IReadOnlyList<BlockEdit> SnapshotBlocks()
    {
        return _blocks.Snapshot();
    }

    internal unsafe int RequestChunkColumns(ChunkColumnRequestFrame* requestFrame)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        return _chunkColumns.RequestChunkColumns(requestFrame);
    }

    internal NativeChunkStreamSnapshotResult WriteChunkStreamProcessSnapshotFile(
        IntPtr streamWriteTracker,
        string streamPath,
        NativeChunkViewIntent intent,
        NativeChunkStreamProcessWritePlan writePlan,
        bool metadataOnly,
        WorldTimeSnapshot worldTime,
        PlayerState playerState)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        return _chunkColumns.WriteProcessSnapshotFile(
            streamWriteTracker,
            streamPath,
            intent,
            writePlan,
            metadataOnly,
            worldTime,
            playerState);
    }

    internal int WorldBlockCount => _blocks.BlockCount;

    internal int PendingClientBlockCommandCount => _clientBlockCommands.PendingCount;

    internal int PendingBlockChangeCount => _blockChanges?.PendingCount ?? 0;

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
            LiveDebugLog.Write($"server_live_activate active=1 module=none blocks={_blocks.BlockCount} pending_block_changes={PendingBlockChangeCount} publication={PublicationMode}");
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
            var serverCommandSink = new BlockCommandSink(_blockEdits, _blockChanges, OnBlocksChanged, commandSink);
            _instance = _registration.CreateInstance(HostModuleContext.Create(_registration.Manifest, serverCommandSink));
            _playerController.AlignSpawnToSurface();
            _blockPersistence.EnsureInitialized(_blocks);
            LiveDebugLog.Write($"server_live_activate active=1 blocks={_blocks.BlockCount} pending_block_changes={PendingBlockChangeCount} publication={PublicationMode}");
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

        AdvanceFluids(frame.DeltaSeconds);
        _blockPersistence.SaveIfDirty(_blocks);
        LiveDebugLog.Write($"server_live_tick frame={frame.FrameIndex} tick={_lastTickId} dt={frame.DeltaSeconds:F6} client_commands_pending_before={pendingClientCommands} client_commands_applied={appliedClientCommands} blocks={_blocks.BlockCount} pending_block_changes={PendingBlockChangeCount}");
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
        AdvanceFluids(frame.DeltaSeconds);
        _blockPersistence.SaveIfDirty(_blocks);
        LiveDebugLog.Write($"server_live_tick frame={frame.FrameIndex} tick={_lastTickId} dt={frame.DeltaSeconds:F6} host_only=1 module={(_registration is null ? "none" : _registration.Manifest.ModuleId)} client_commands_pending_before={pendingClientCommands} client_commands_applied={appliedClientCommands} blocks={_blocks.BlockCount} pending_block_changes={PendingBlockChangeCount}");
    }

    internal unsafe int SubmitClientCommands(HostCommand* commands, uint commandCount)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        return _clientBlockCommands.SubmitAndLog(commands, commandCount);
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
        return _blockChanges is null ? DeltaSnapshotsUnsupported
            : _blockChanges.DrainSnapshotReportAndLog(snapshotHeader, _lastTickId);
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
            _fluids?.Dispose();
            _blockPersistence.SaveIfDirty(_blocks);
            _clientBlockCommands.Dispose();
            _scheduleRuntime.Dispose();
            _blockPersistence.Dispose();
            _worldTime.Dispose();
            _blockChanges?.Dispose();
            _blocks.Dispose();
            _instance = null;
        }
    }

    private void OnBlocksChanged(IReadOnlyList<BlockEdit> changes)
    {
        MarkBlockPersistenceDirty(changes.Count);
        _fluids?.Wake(changes);
    }

    private void AdvanceFluids(double deltaSeconds)
    {
        if (_fluids is null) return;
        _scheduleRuntime.ExecuteCommandWriteMainThread("server.fluid.tick", () =>
        {
            var started = System.Diagnostics.Stopwatch.GetTimestamp();
            FluidReport = _fluids.Advance(deltaSeconds, _blockEdits, _blockChanges);
            FluidStepMilliseconds = System.Diagnostics.Stopwatch.GetElapsedTime(started).TotalMilliseconds;
            if (FluidReport.Changed == 0) return;
            MarkBlockPersistenceDirty(checked((int)FluidReport.Changed));
            LiveDebugLog.Write($"server_live_fluid_tick changed={FluidReport.Changed} pending={FluidReport.Pending} evaluations={FluidReport.Evaluations} reads={FluidReport.Reads} now_ms={FluidReport.NowMs} step_ms={FluidStepMilliseconds:F3}");
        });
    }

    private void MarkBlockPersistenceDirty(int editCount)
    {
        if (editCount <= 0) return;
        BlockRevision++;
        LiveDebugLog.Write($"server_live_block_persistence_dirty edits={editCount}");
        _blockPersistence.MarkDirty();
    }

}
