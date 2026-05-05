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
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>, void*, NativeBlockEdit*, uint, uint*, NativeBlockEditResult> BlockEditServiceApply;

    public static readonly delegate* unmanaged[Cdecl]<IntPtr> BlockChangeQueueCreate;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> BlockChangeQueueDestroy;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong> BlockChangeQueuePendingCount;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, void> BlockChangeQueueEnqueue;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ReplicationChange*, uint, ulong, uint*, int> BlockChangeQueueDrain;

    public static readonly delegate* unmanaged[Cdecl]<IntPtr> ClientBlockCommandQueueCreate;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, void> ClientBlockCommandQueueDestroy;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong> ClientBlockCommandQueuePendingCount;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, HostCommand*, uint>, void*, uint> ClientBlockCommandQueueCanQueue;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, HostCommand*, uint>, void*, uint> ClientBlockCommandQueueEnqueue;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, delegate* unmanaged[Cdecl]<void*, HostCommand*, uint>, void*, int> ClientBlockCommandQueueDrain;
    public static readonly delegate* unmanaged[Cdecl]<HostCommand*, NativeBlockPosition*, uint> ClientBlockCommandHitPosition;
    public static readonly delegate* unmanaged[Cdecl]<HostCommand*, ushort, ushort, uint> ClientBlockCommandIsValidInteraction;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, uint, int, int, uint, uint, NativeChunkStreamCounts*, int> ChunkStreamCount;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, uint, int, int, uint, uint, NativeChunkWindowEvent*, uint, NativeChunkStreamColumn*, uint, NativeChunkStreamBlock*, uint, NativeChunkStreamCounts*, int> ChunkStreamFill;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeChunkStreamSnapshotRequest*, NativeChunkStreamSnapshotResult*, int> ChunkStreamWriteSnapshotFile;
    public static readonly delegate* unmanaged[Cdecl]<IntPtr, ChunkColumnRequestFrame*, int> ChunkStreamRequestColumns;

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
        BlockEditServiceApply = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, delegate* unmanaged[Cdecl]<void*, NativeBlockPosition*, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, NativeBlockEdit*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, ushort, NativeBlockPosition*, ushort, uint>, void*, NativeBlockEdit*, uint, uint*, NativeBlockEditResult>)Export(library, "octaryn_server_block_edit_service_apply");

        BlockChangeQueueCreate = (delegate* unmanaged[Cdecl]<IntPtr>)Export(library, "octaryn_server_block_change_queue_create");
        BlockChangeQueueDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)Export(library, "octaryn_server_block_change_queue_destroy");
        BlockChangeQueuePendingCount = (delegate* unmanaged[Cdecl]<IntPtr, ulong>)Export(library, "octaryn_server_block_change_queue_pending_count");
        BlockChangeQueueEnqueue = (delegate* unmanaged[Cdecl]<IntPtr, NativeBlockEdit*, void>)Export(library, "octaryn_server_block_change_queue_enqueue");
        BlockChangeQueueDrain = (delegate* unmanaged[Cdecl]<IntPtr, ReplicationChange*, uint, ulong, uint*, int>)Export(library, "octaryn_server_block_change_queue_drain");

        ClientBlockCommandQueueCreate = (delegate* unmanaged[Cdecl]<IntPtr>)Export(library, "octaryn_server_client_block_command_queue_create");
        ClientBlockCommandQueueDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)Export(library, "octaryn_server_client_block_command_queue_destroy");
        ClientBlockCommandQueuePendingCount = (delegate* unmanaged[Cdecl]<IntPtr, ulong>)Export(library, "octaryn_server_client_block_command_queue_pending_count");
        ClientBlockCommandQueueCanQueue = (delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, HostCommand*, uint>, void*, uint>)Export(library, "octaryn_server_client_block_command_queue_can_queue");
        ClientBlockCommandQueueEnqueue = (delegate* unmanaged[Cdecl]<IntPtr, HostCommand*, delegate* unmanaged[Cdecl]<void*, ushort, uint>, delegate* unmanaged[Cdecl]<void*, HostCommand*, uint>, void*, uint>)Export(library, "octaryn_server_client_block_command_queue_enqueue");
        ClientBlockCommandQueueDrain = (delegate* unmanaged[Cdecl]<IntPtr, delegate* unmanaged[Cdecl]<void*, HostCommand*, uint>, void*, int>)Export(library, "octaryn_server_client_block_command_queue_drain");
        ClientBlockCommandHitPosition = (delegate* unmanaged[Cdecl]<HostCommand*, NativeBlockPosition*, uint>)Export(library, "octaryn_server_client_block_command_hit_position");
        ClientBlockCommandIsValidInteraction = (delegate* unmanaged[Cdecl]<HostCommand*, ushort, ushort, uint>)Export(library, "octaryn_server_client_block_command_is_valid_interaction");
        ChunkStreamCount = (delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, uint, int, int, uint, uint, NativeChunkStreamCounts*, int>)Export(library, "octaryn_server_chunk_stream_count");
        ChunkStreamFill = (delegate* unmanaged[Cdecl]<IntPtr, int, int, uint, uint, int, int, uint, uint, NativeChunkWindowEvent*, uint, NativeChunkStreamColumn*, uint, NativeChunkStreamBlock*, uint, NativeChunkStreamCounts*, int>)Export(library, "octaryn_server_chunk_stream_fill");
        ChunkStreamWriteSnapshotFile = (delegate* unmanaged[Cdecl]<IntPtr, NativeChunkStreamSnapshotRequest*, NativeChunkStreamSnapshotResult*, int>)Export(library, "octaryn_server_chunk_stream_write_snapshot_file");
        ChunkStreamRequestColumns = (delegate* unmanaged[Cdecl]<IntPtr, ChunkColumnRequestFrame*, int>)Export(library, "octaryn_server_chunk_stream_request_columns");
    }

    private static IntPtr Export(IntPtr library, string name)
    {
        return NativeLibrary.GetExport(library, name);
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
