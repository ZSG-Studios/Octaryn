using System.Runtime.InteropServices;

namespace Octaryn.Server.Persistence.WorldBlocks;

internal static unsafe partial class NativeWorldPersistenceLibrary
{
    private const string LibraryName = "octaryn_server_world_persistence";

    private static readonly delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int> s_planChunkColumnsCount;
    private static readonly delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int> s_planChunkColumnsFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideFile*, int> s_readChunkOverrideFileCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideBlock*, uint, NativePersistenceChunkOverrideFile*, int> s_readChunkOverrideFileFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideFile*, NativePersistenceChunkOverrideBlock*, int> s_writeChunkOverrideFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldBlockOverrideFile*, int> s_readWorldBlockOverrideFileCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceBlockEdit*, uint, NativePersistenceWorldBlockOverrideFile*, int> s_readWorldBlockOverrideFileFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, uint*, int> s_countWorldBlockOverrideColumns;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldBlockOverrideFile*, NativePersistenceBlockEdit*, int> s_writeWorldBlockOverrideFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, uint*, int> s_readWorldBlockOverridesCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceBlockEdit*, uint, uint*, int> s_readWorldBlockOverridesFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceBlockEdit*, uint, int> s_initializeWorldBlockOverrides;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceBlockEdit*, uint, int> s_saveWorldBlockOverrides;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr> s_worldBlockSaveTrackerCreate;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, void> s_worldBlockSaveTrackerDestroy;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, void> s_worldBlockSaveTrackerMarkDirty;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, uint> s_worldBlockSaveTrackerShouldSave;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, void> s_worldBlockSaveTrackerMarkClean;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceChunkOverrideDirectoryScan*, int> s_scanChunkOverrideDirectory;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, uint*, int> s_readChunkOverrideDirectoryCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceBlockEdit*, uint, uint*, int> s_readChunkOverrideDirectoryFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkColumn*, uint, uint*, int> s_pruneStaleChunkOverrideFiles;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, int> s_writeChunkOverrideDirectory;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistencePlanCounts*, int> s_planWorldBlockExportColumnsCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int> s_planWorldBlockExportColumnsFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, int> s_writeGzipFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, ulong*, int> s_readGzipFileCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, ulong*, int> s_readGzipFileFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int> s_readPlayerFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int> s_writePlayerFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, uint*, int> s_readPlayerDirectoryCount;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerFileEntry*, uint, uint*, int> s_readPlayerDirectoryFill;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, int, NativePersistencePlayerState*, int> s_readPlayerDirectoryEntry;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, int, NativePersistencePlayerState*, int> s_writePlayerDirectoryEntry;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldTimeState*, int> s_readWorldTimeFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldTimeState*, int> s_writeWorldTimeFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldMetadata*, int> s_readWorldMetadataFile;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldMetadata*, int> s_writeWorldMetadataFile;

    static NativeWorldPersistenceLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        s_planChunkColumnsCount = (delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_chunk_columns_count");
        s_planChunkColumnsFill = (delegate* unmanaged[Cdecl]<NativePersistenceBlockEdit*, uint, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_chunk_columns_fill");
        s_readChunkOverrideFileCount = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideFile*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_chunk_override_file_count");
        s_readChunkOverrideFileFill = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideBlock*, uint, NativePersistenceChunkOverrideFile*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_chunk_override_file_fill");
        s_writeChunkOverrideFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkOverrideFile*, NativePersistenceChunkOverrideBlock*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_chunk_override_file");
        s_readWorldBlockOverrideFileCount = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldBlockOverrideFile*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_world_block_override_file_count");
        s_readWorldBlockOverrideFileFill = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceBlockEdit*, uint, NativePersistenceWorldBlockOverrideFile*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_world_block_override_file_fill");
        s_countWorldBlockOverrideColumns = (delegate* unmanaged[Cdecl]<IntPtr, uint*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_count_world_block_override_columns");
        s_writeWorldBlockOverrideFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldBlockOverrideFile*, NativePersistenceBlockEdit*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_world_block_override_file");
        s_readWorldBlockOverridesCount = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, uint*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_world_block_overrides_count");
        s_readWorldBlockOverridesFill = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceBlockEdit*, uint, uint*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_world_block_overrides_fill");
        s_initializeWorldBlockOverrides = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceBlockEdit*, uint, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_initialize_world_block_overrides");
        s_saveWorldBlockOverrides = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceBlockEdit*, uint, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_save_world_block_overrides");
        s_worldBlockSaveTrackerCreate = (delegate* unmanaged[Cdecl]<IntPtr>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_world_block_save_tracker_create");
        s_worldBlockSaveTrackerDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_world_block_save_tracker_destroy");
        s_worldBlockSaveTrackerMarkDirty = (delegate* unmanaged[Cdecl]<IntPtr, void>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_world_block_save_tracker_mark_dirty");
        s_worldBlockSaveTrackerShouldSave = (delegate* unmanaged[Cdecl]<IntPtr, uint>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_world_block_save_tracker_should_save");
        s_worldBlockSaveTrackerMarkClean = (delegate* unmanaged[Cdecl]<IntPtr, void>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_world_block_save_tracker_mark_clean");
        s_scanChunkOverrideDirectory = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceChunkOverrideDirectoryScan*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_scan_chunk_override_directory");
        s_readChunkOverrideDirectoryCount = (delegate* unmanaged[Cdecl]<IntPtr, uint*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_chunk_override_directory_count");
        s_readChunkOverrideDirectoryFill = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceBlockEdit*, uint, uint*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_chunk_override_directory_fill");
        s_pruneStaleChunkOverrideFiles = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkColumn*, uint, uint*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_prune_stale_chunk_override_files");
        s_writeChunkOverrideDirectory = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_chunk_override_directory");
        s_planWorldBlockExportColumnsCount = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_world_block_export_columns_count");
        s_planWorldBlockExportColumnsFill = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, NativePersistenceChunkColumn*, uint, NativePersistenceBlockEdit*, uint, NativePersistencePlanCounts*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_plan_world_block_export_columns_fill");
        s_writeGzipFile = (delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_gzip_file");
        s_readGzipFileCount = (delegate* unmanaged[Cdecl]<IntPtr, ulong*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_gzip_file_count");
        s_readGzipFileFill = (delegate* unmanaged[Cdecl]<IntPtr, byte*, ulong, ulong*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_gzip_file_fill");
        s_readPlayerFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_player_file");
        s_writePlayerFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_player_file");
        s_readPlayerDirectoryCount = (delegate* unmanaged[Cdecl]<IntPtr, uint*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_player_directory_count");
        s_readPlayerDirectoryFill = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistencePlayerFileEntry*, uint, uint*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_player_directory_fill");
        s_readPlayerDirectoryEntry = (delegate* unmanaged[Cdecl]<IntPtr, int, NativePersistencePlayerState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_player_directory_entry");
        s_writePlayerDirectoryEntry = (delegate* unmanaged[Cdecl]<IntPtr, int, NativePersistencePlayerState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_player_directory_entry");
        s_readWorldTimeFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldTimeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_world_time_file");
        s_writeWorldTimeFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldTimeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_world_time_file");
        s_readWorldMetadataFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldMetadata*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_read_world_metadata_file");
        s_writeWorldMetadataFile = (delegate* unmanaged[Cdecl]<IntPtr, NativePersistenceWorldMetadata*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_persistence_write_world_metadata_file");
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_PERSISTENCE_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeWorldPersistenceLibrary).Assembly.Location;
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
