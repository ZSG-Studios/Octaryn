using Octaryn.Server.Modules;
using Octaryn.Server.World.Blocks;
using Octaryn.Server.World.Chunks;

namespace Octaryn.Server;

internal static unsafe partial class ChunkStreamProcessBridge
{
    internal static int PublishSnapshot(ModuleActivator gameModule, string streamPath,
 NativeChunkViewIntent intent, bool metadataOnly, bool submittedBlockCommands)
    {
        var publication = gameModule.ChunkPublication;
        var revision = gameModule.BlockRevision;
        var publicationRequested = publication.ShouldPublish(streamPath, intent, revision, submittedBlockCommands);
 if (!publicationRequested)
            return 0;

        // The process binary format replaces client overrides and has no delta flag.
        // Even an epoch-only publication must retain all existing override records.
        const bool effectiveMetadataOnly = false;
        // Plan publication after the authority tick; never execute another tick here.
        var stagePlan = default(NativeChunkStreamProcessStagePlan);
        if (NativeBlockStoreLibrary.ChunkStreamPlanProcessStage(StreamWriteTracker, &intent, 0u,
                publicationRequested || submittedBlockCommands ? 1u : 0u,
                effectiveMetadataOnly ? 1u : 0u, &stagePlan) != 0)
            return -1;
        var writePlan = stagePlan.Write;
        if (writePlan.ShouldContinue == 0) return writePlan.HandleResult;
        if (writePlan.ShouldWrite == 0)
        {
            LiveDebugLog.Write($"server_live_chunk_stream active=1 skipped=1 reason=unchanged_window epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius}");
            return 0;
        }

        var worldTime = gameModule.SnapshotWorldTime();
        var player = gameModule.SnapshotPlayer();
        var publishIntent = intent;
        var writeResult = publication.Write(streamPath, intent, revision, effectiveMetadataOnly,
            () => gameModule.WriteChunkStreamProcessSnapshotFile(
                StreamWriteTracker, streamPath, publishIntent, writePlan, effectiveMetadataOnly, worldTime, player));
        LiveDebugLog.Write($"server_live_chunk_window epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius} load={writeResult.LoadCount} preserve={writeResult.PreserveCount} unload={writeResult.UnloadCount}");
        LiveDebugLog.Write($"server_live_chunk_stream active=1 source=process_file path={streamPath} epoch={intent.Epoch} center=({intent.CenterChunkX},{intent.CenterChunkZ}) radius={intent.Radius} columns={writeResult.Counts.ColumnCount} blocks={writeResult.Counts.BlockCount} metadata_only={(effectiveMetadataOnly ? 1 : 0)} requested_metadata_only={(metadataOnly ? 1 : 0)} command_delta={(submittedBlockCommands ? 1 : 0)} publication_requested={(publicationRequested ? 1 : 0)} revision={revision} world_time_day_fraction={worldTime.DayFraction:F6}");
        return 0;
    }
}
