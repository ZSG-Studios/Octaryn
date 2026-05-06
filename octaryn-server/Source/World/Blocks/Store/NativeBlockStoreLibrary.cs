using System.Runtime.InteropServices;
using Octaryn.Server.World.Chunks;
using Octaryn.Shared.Host;
using Octaryn.Shared.Networking;

namespace Octaryn.Server.World.Blocks;

internal static unsafe class NativeBlockStoreLibrary
{
    private const string LibraryName = "octaryn_server_block_store";

    public static readonly delegate* unmanaged[Cdecl]<IntPtr> BlockStoreCreate;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> BlockStoreDestroy;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong> BlockStoreBlockCount;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockPosition*, ushort> BlockStoreGetBlock;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockPosition*, ushort*, uint> BlockStoreTryGetBlock;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockPosition*, NativeBlockEditResult> BlockStoreClearBlockOverride;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, uint, NativeBlockEditResult> BlockStoreSetBlock;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong> BlockStoreSnapshotCount;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, ulong, ulong> BlockStoreSnapshotFill;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, int, ulong> BlockStoreSnapshotChunkColumnCount;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, int, NativeBlockEdit*, ulong, ulong> BlockStoreSnapshotChunkColumnFill;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, ulong, void> BlockStoreLoad;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, void*, int> BlockStoreClearOverridesMatching;
    public static readonly delegate* unmanaged[Cdecl]<NativeBlockPosition*, uint> BlockStoreIsValidPosition;
    public static readonly delegate* unmanaged[Cdecl]<NativeBlockPosition*, NativeChunkPosition> BlockStoreChunkPositionFor;
    public static readonly delegate* unmanaged[Cdecl]<NativeBlockPosition*, NativeBlockPosition> BlockStoreLocalPositionFor;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, void*, uint> BlockEditServiceCanApply;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, void*, uint> BlockEditServiceCanApplyCommand;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>, void*, NativeBlockEdit*, uint, uint*, NativeBlockEditResult> BlockEditServiceApply;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>, void*, NativeBlockEdit*, uint, uint*, NativeBlockEditResult> BlockEditServiceApplyCommand;

    public static readonly delegate* unmanaged[Cdecl]<IntPtr> BlockChangeQueueCreate;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> BlockChangeQueueDestroy;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong> BlockChangeQueuePendingCount;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, void> BlockChangeQueueEnqueue;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ReplicationChange*, uint, ulong, uint*, int> BlockChangeQueueDrain;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ServerSnapshotHeader*, ulong, ulong*, uint*, int> BlockChangeQueueDrainSnapshot;

    public static readonly delegate* unmanaged[Cdecl]<IntPtr> ClientBlockCommandQueueCreate;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> ClientBlockCommandQueueDestroy;
    public static readonly delegate* unmanaged[Cdecl]<ulong> ClientBlockCommandQueueMaxPending;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong> ClientBlockCommandQueuePendingCount;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, uint, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, HostCommand*, uint>, void*, NativeClientBlockCommandSubmitReport*, int> ClientBlockCommandQueueSubmitReport;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>, void*, delegate* unmanaged[Cdecl]<void*, HostCommand*, NativeBlockEditResult*, NativeBlockEdit*, uint, uint>, void*, int> ClientBlockCommandQueueDrainApply;
    public static readonly delegate* unmanaged[Cdecl]<HostCommand*, NativeBlockPosition*, uint> ClientBlockCommandHitPosition;
    public static readonly delegate* unmanaged[Cdecl]<HostCommand*, ushort, ushort, uint> ClientBlockCommandIsValidInteraction;
    public static readonly delegate* unmanaged[Cdecl]<HostCommand*, byte*> ClientBlockCommandEditLabel;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, uint, int, int, uint, uint, NativeChunkStreamCounts*, int> ChunkStreamCount;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, uint, int, int, uint, uint, NativeChunkWindowEvent*, uint, NativeChunkStreamColumn*, uint, NativeChunkStreamBlock*, uint, NativeChunkStreamCounts*, int> ChunkStreamFill;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeChunkStreamSnapshotRequest*, NativeChunkStreamSnapshotResult*, int> ChunkStreamWriteSnapshotFile;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ChunkColumnRequestFrame*, int> ChunkStreamRequestColumns;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, uint, ChunkColumnRequestFrame*, int> ChunkStreamRequestColumnsIfAvailable;
    public static readonly delegate* unmanaged[Cdecl]<byte*, NativeChunkViewIntent*, int> ChunkStreamReadViewIntent;
    public static readonly delegate* unmanaged[Cdecl]<byte*, uint, NativeChunkViewIntent*, NativeChunkStreamProcessWritePlan*, int> ChunkStreamReadProcessIntent;
    public static readonly delegate* unmanaged[Cdecl]<byte*, HostCommand*, uint, NativeBlockInteractionIntentResult*, int> BlockInteractionReadIntentFile;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr> BlockInteractionFrameTrackerCreate;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> BlockInteractionFrameTrackerDestroy;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, uint, NativeBlockInteractionIntentResult*, NativeBlockInteractionProcessPlan*, int> BlockInteractionPlanProcessIntent;
    public static readonly delegate* unmanaged[Cdecl]<uint, byte*> BlockInteractionProcessReasonName;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong, void> BlockInteractionFrameTrackerNoteSubmitted;
    public static readonly delegate* unmanaged[Cdecl]<ChunkColumnRequestFrame*, uint, uint, uint, int> ChunkStreamWriteRequestResult;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr> ChunkStreamWriteTrackerCreate;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> ChunkStreamWriteTrackerDestroy;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, uint, uint, int, int, uint, uint, int, int, uint, NativeChunkStreamWriteDecision> ChunkStreamWriteTrackerDecide;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, void> ChunkStreamWriteTrackerNoteWritten;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, uint, NativeChunkViewIntent*, uint, uint, NativeChunkStreamProcessWritePlan*, int> ChunkStreamPlanProcessWrite;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeChunkStreamProcessWritePlan*, void> ChunkStreamProcessWritePlanNoteWritten;
    public static readonly delegate* unmanaged[Cdecl]<uint, int, byte*> ChunkStreamProcessWriteReasonName;
    public static readonly delegate* unmanaged[Cdecl]<uint, uint, uint, NativeChunkStreamProcessTickDecision> ChunkStreamDecideProcessTick;
    public static readonly delegate* unmanaged[Cdecl]<HostFrameSnapshot*, int> ChunkStreamCreateProcessFrame;

    static NativeBlockStoreLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        BlockStoreCreate = (delegate* unmanaged[Cdecl]<IntPtr>)Export(library, "octaryn_server_block_store_create");
        BlockStoreDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)Export(library, "octaryn_server_block_store_destroy");
        BlockStoreBlockCount = (delegate* unmanaged[Cdecl]<IntPtr, ulong>)Export(library, "octaryn_server_block_store_block_count");
        BlockStoreGetBlock = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockPosition*, ushort>)Export(library, "octaryn_server_block_store_get_block");
        BlockStoreTryGetBlock = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockPosition*, ushort*, uint>)Export(library, "octaryn_server_block_store_try_get_block");
        BlockStoreClearBlockOverride = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockPosition*, NativeBlockEditResult>)Export(library, "octaryn_server_block_store_clear_block_override");
        BlockStoreSetBlock = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, uint, NativeBlockEditResult>)Export(library, "octaryn_server_block_store_set_block");
        BlockStoreSnapshotCount = (delegate* unmanaged[Cdecl]<IntPtr, ulong>)Export(library, "octaryn_server_block_store_snapshot_count");
        BlockStoreSnapshotFill = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, ulong, ulong>)Export(library, "octaryn_server_block_store_snapshot_fill");
        BlockStoreSnapshotChunkColumnCount = (delegate* unmanaged[Cdecl]<IntPtr, int, int, ulong>)Export(library, "octaryn_server_block_store_snapshot_chunk_column_count");
        BlockStoreSnapshotChunkColumnFill = (delegate* unmanaged[Cdecl]<IntPtr, int, int, NativeBlockEdit*, ulong, ulong>)Export(library, "octaryn_server_block_store_snapshot_chunk_column_fill");
        BlockStoreLoad = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, ulong, void>)Export(library, "octaryn_server_block_store_load");
        BlockStoreClearOverridesMatching = (delegate* unmanaged[Cdecl]<IntPtr, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, void*, int>)Export(library, "octaryn_server_block_store_clear_overrides_matching");
        BlockStoreIsValidPosition = (delegate* unmanaged[Cdecl]<NativeBlockPosition*, uint>)Export(library, "octaryn_server_block_store_is_valid_position");
        BlockStoreChunkPositionFor = (delegate* unmanaged[Cdecl]<NativeBlockPosition*, NativeChunkPosition>)Export(library, "octaryn_server_block_store_chunk_position_for");
        BlockStoreLocalPositionFor = (delegate* unmanaged[Cdecl]<NativeBlockPosition*, NativeBlockPosition>)Export(library, "octaryn_server_block_store_local_position_for");
        BlockEditServiceCanApply = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, void*, uint>)Export(library, "octaryn_server_block_edit_service_can_apply");
        BlockEditServiceCanApplyCommand = (delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, void*, uint>)Export(library, "octaryn_server_block_edit_service_can_apply_command");
        BlockEditServiceApply = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>, void*, NativeBlockEdit*, uint, uint*, NativeBlockEditResult>)Export(library, "octaryn_server_block_edit_service_apply");
        BlockEditServiceApplyCommand = (delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>, void*, NativeBlockEdit*, uint, uint*, NativeBlockEditResult>)Export(library, "octaryn_server_block_edit_service_apply_command");

        BlockChangeQueueCreate = (delegate* unmanaged[Cdecl]<IntPtr>)Export(library, "octaryn_server_block_change_queue_create");
        BlockChangeQueueDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)Export(library, "octaryn_server_block_change_queue_destroy");
        BlockChangeQueuePendingCount = (delegate* unmanaged[Cdecl]<IntPtr, ulong>)Export(library, "octaryn_server_block_change_queue_pending_count");
        BlockChangeQueueEnqueue = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, void>)Export(library, "octaryn_server_block_change_queue_enqueue");
        BlockChangeQueueDrain = (delegate* unmanaged[Cdecl]<IntPtr, ReplicationChange*, uint, ulong, uint*, int>)Export(library, "octaryn_server_block_change_queue_drain");
        BlockChangeQueueDrainSnapshot = (delegate* unmanaged[Cdecl]<IntPtr, ServerSnapshotHeader*, ulong, ulong*, uint*, int>)Export(library, "octaryn_server_block_change_queue_drain_snapshot");

        ClientBlockCommandQueueCreate = (delegate* unmanaged[Cdecl]<IntPtr>)Export(library, "octaryn_server_client_block_command_queue_create");
        ClientBlockCommandQueueDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)Export(library, "octaryn_server_client_block_command_queue_destroy");
        ClientBlockCommandQueueMaxPending = (delegate* unmanaged[Cdecl]<ulong>)Export(library, "octaryn_server_client_block_command_queue_max_pending");
        ClientBlockCommandQueuePendingCount = (delegate* unmanaged[Cdecl]<IntPtr, ulong>)Export(library, "octaryn_server_client_block_command_queue_pending_count");
        ClientBlockCommandQueueSubmitReport = (delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, uint, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, HostCommand*, uint>, void*, NativeClientBlockCommandSubmitReport*, int>)Export(library, "octaryn_server_client_block_command_queue_submit_report");
        ClientBlockCommandQueueDrainApply = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>, void*, delegate* unmanaged[Cdecl]<void*, HostCommand*, NativeBlockEditResult*, NativeBlockEdit*, uint, uint>, void*, int>)Export(library, "octaryn_server_client_block_command_queue_drain_apply");
        ClientBlockCommandHitPosition = (delegate* unmanaged[Cdecl]<HostCommand*, NativeBlockPosition*, uint>)Export(library, "octaryn_server_client_block_command_hit_position");
        ClientBlockCommandIsValidInteraction = (delegate* unmanaged[Cdecl]<HostCommand*, ushort, ushort, uint>)Export(library, "octaryn_server_client_block_command_is_valid_interaction");
        ClientBlockCommandEditLabel = (delegate* unmanaged[Cdecl]<HostCommand*, byte*>)Export(library, "octaryn_server_client_block_command_edit_label");
        ChunkStreamCount = (delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, uint, int, int, uint, uint, NativeChunkStreamCounts*, int>)Export(library, "octaryn_server_chunk_stream_count");
        ChunkStreamFill = (delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, uint, int, int, uint, uint, NativeChunkWindowEvent*, uint, NativeChunkStreamColumn*, uint, NativeChunkStreamBlock*, uint, NativeChunkStreamCounts*, int>)Export(library, "octaryn_server_chunk_stream_fill");
        ChunkStreamWriteSnapshotFile = (delegate* unmanaged[Cdecl]<IntPtr, NativeChunkStreamSnapshotRequest*, NativeChunkStreamSnapshotResult*, int>)Export(library, "octaryn_server_chunk_stream_write_snapshot_file");
        ChunkStreamRequestColumns = (delegate* unmanaged[Cdecl]<IntPtr, ChunkColumnRequestFrame*, int>)Export(library, "octaryn_server_chunk_stream_request_columns");
        ChunkStreamRequestColumnsIfAvailable = (delegate* unmanaged[Cdecl]<IntPtr, uint, ChunkColumnRequestFrame*, int>)Export(library, "octaryn_server_chunk_stream_request_columns_if_available");
        ChunkStreamReadViewIntent = (delegate* unmanaged[Cdecl]<byte*, NativeChunkViewIntent*, int>)Export(library, "octaryn_server_chunk_stream_read_view_intent");
        ChunkStreamReadProcessIntent = (delegate* unmanaged[Cdecl]<byte*, uint, NativeChunkViewIntent*, NativeChunkStreamProcessWritePlan*, int>)Export(library, "octaryn_server_chunk_stream_read_process_intent");
        BlockInteractionReadIntentFile = (delegate* unmanaged[Cdecl]<byte*, HostCommand*, uint, NativeBlockInteractionIntentResult*, int>)Export(library, "octaryn_server_block_interaction_read_intent_file");
        BlockInteractionFrameTrackerCreate = (delegate* unmanaged[Cdecl]<IntPtr>)Export(library, "octaryn_server_block_interaction_frame_tracker_create");
        BlockInteractionFrameTrackerDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)Export(library, "octaryn_server_block_interaction_frame_tracker_destroy");
        BlockInteractionPlanProcessIntent = (delegate* unmanaged[Cdecl]<IntPtr, int, uint, NativeBlockInteractionIntentResult*, NativeBlockInteractionProcessPlan*, int>)Export(library, "octaryn_server_block_interaction_plan_process_intent");
        BlockInteractionProcessReasonName = (delegate* unmanaged[Cdecl]<uint, byte*>)Export(library, "octaryn_server_block_interaction_process_reason_name");
        BlockInteractionFrameTrackerNoteSubmitted = (delegate* unmanaged[Cdecl]<IntPtr, ulong, void>)Export(library, "octaryn_server_block_interaction_frame_tracker_note_submitted");
        ChunkStreamWriteRequestResult = (delegate* unmanaged[Cdecl]<ChunkColumnRequestFrame*, uint, uint, uint, int>)Export(library, "octaryn_server_chunk_stream_write_request_result");
        ChunkStreamWriteTrackerCreate = (delegate* unmanaged[Cdecl]<IntPtr>)Export(library, "octaryn_server_chunk_stream_write_tracker_create");
        ChunkStreamWriteTrackerDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)Export(library, "octaryn_server_chunk_stream_write_tracker_destroy");
        ChunkStreamWriteTrackerDecide = (delegate* unmanaged[Cdecl]<IntPtr, uint, uint, int, int, uint, uint, int, int, uint, NativeChunkStreamWriteDecision>)Export(library, "octaryn_server_chunk_stream_write_tracker_decide");
        ChunkStreamWriteTrackerNoteWritten = (delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, void>)Export(library, "octaryn_server_chunk_stream_write_tracker_note_written");
        ChunkStreamPlanProcessWrite = (delegate* unmanaged[Cdecl]<IntPtr, int, uint, NativeChunkViewIntent*, uint, uint, NativeChunkStreamProcessWritePlan*, int>)Export(library, "octaryn_server_chunk_stream_plan_process_write");
        ChunkStreamProcessWritePlanNoteWritten = (delegate* unmanaged[Cdecl]<IntPtr, NativeChunkStreamProcessWritePlan*, void>)Export(library, "octaryn_server_chunk_stream_process_write_plan_note_written");
        ChunkStreamProcessWriteReasonName = (delegate* unmanaged[Cdecl]<uint, int, byte*>)Export(library, "octaryn_server_chunk_stream_process_write_reason_name");
        ChunkStreamDecideProcessTick = (delegate* unmanaged[Cdecl]<uint, uint, uint, NativeChunkStreamProcessTickDecision>)Export(library, "octaryn_server_chunk_stream_decide_process_tick");
        ChunkStreamCreateProcessFrame = (delegate* unmanaged[Cdecl]<HostFrameSnapshot*, int>)Export(library, "octaryn_server_chunk_stream_create_process_frame");
    }

    private static IntPtr Export(IntPtr library, string name)
    {
        return NativeLibrary.GetExport(library, name);
    }

    public static string HostCommandEditLabel(HostCommand command)
    {
        var nativeCommand = command;
        return Marshal.PtrToStringUTF8((IntPtr)ClientBlockCommandEditLabel(&nativeCommand)) ?? "none";
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_BLOCK_STORE_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeBlockStoreLibrary).Assembly.Location;
        if (!string.IsNullOrWhiteSpace(assemblyPath))
        {
            var assemblyLibraryPath = Path.Combine(Path.GetDirectoryName(assemblyPath) ?? string.Empty, fileName);
            if (File.Exists(assemblyLibraryPath))
            {
                return assemblyLibraryPath;
            }
        }

        var bundledPath = Path.Combine(AppContext.BaseDirectory, fileName);
        return File.Exists(bundledPath) ? bundledPath : LibraryName;
    }
}
