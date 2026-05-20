using System.Runtime.InteropServices;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Generation;

internal static unsafe class NativeTerrainGenerationLibrary
{
    private const string LibraryName = "octaryn_server_terrain_generation";

    private static readonly delegate* unmanaged[Cdecl]<int, int, int, NativeTerrainMaterialRules*, ushort*, int> TerrainGeneratedBlock;
    private static readonly delegate* unmanaged[Cdecl]<int, int, int, ushort> EmptyWorldGeneratedBlockPointer;
    private static readonly delegate* unmanaged[Cdecl]<int, int, int, NativeTerrainMaterialRules*, ushort> FlatTestGeneratedBlockPointer;
    private static readonly delegate* unmanaged[Cdecl]<ushort> EmptyWorldWhiteBlockPointer;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeTerrainMaterialRules*, int> ClearTerrainMatchingOverridesPointer;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, int> ClearEmptyWorldMatchingOverridesPointer;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeTerrainMaterialRules*, int> ClearFlatTestMatchingOverridesPointer;

    public static BlockId EmptyWorldWhiteBlock => new(EmptyWorldWhiteBlockPointer());

    static NativeTerrainGenerationLibrary()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        TerrainGeneratedBlock = (delegate* unmanaged[Cdecl]<int, int, int, NativeTerrainMaterialRules*, ushort*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_terrain_generated_block");
        EmptyWorldGeneratedBlockPointer = (delegate* unmanaged[Cdecl]<int, int, int, ushort>)NativeLibrary.GetExport(
            library,
            "octaryn_server_empty_world_generated_block");
        FlatTestGeneratedBlockPointer = (delegate* unmanaged[Cdecl]<int, int, int, NativeTerrainMaterialRules*, ushort>)NativeLibrary.GetExport(
            library,
            "octaryn_server_flat_test_generated_block");
        EmptyWorldWhiteBlockPointer = (delegate* unmanaged[Cdecl]<ushort>)NativeLibrary.GetExport(
            library,
            "octaryn_server_empty_world_white_block");
        ClearTerrainMatchingOverridesPointer = (delegate* unmanaged[Cdecl]<IntPtr, NativeTerrainMaterialRules*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_terrain_clear_matching_overrides");
        ClearEmptyWorldMatchingOverridesPointer = (delegate* unmanaged[Cdecl]<IntPtr, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_empty_world_clear_matching_overrides");
        ClearFlatTestMatchingOverridesPointer = (delegate* unmanaged[Cdecl]<IntPtr, NativeTerrainMaterialRules*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_flat_test_clear_matching_overrides");
    }

    public static NativeTerrainMaterialRules MaterialRulesFrom(IWorldGenerationRules rules)
    {
        return new NativeTerrainMaterialRules(
            rules.WaterHeight,
            rules.WaterBlock.Value,
            rules.Materials.SandBlock.Value,
            rules.Materials.GrassBlock.Value,
            rules.Materials.DirtBlock.Value,
            rules.Materials.StoneBlock.Value,
            rules.Materials.SnowBlock.Value);
    }

    public static BlockId GeneratedBlock(BlockPosition position, in NativeTerrainMaterialRules rules)
    {
        ushort block = 0;
        var nativeRules = rules;
        var result = TerrainGeneratedBlock(
            position.X,
            position.Y,
            position.Z,
            &nativeRules,
            &block);
        if (result != 0)
        {
            throw new InvalidOperationException("Native terrain generation failed.");
        }

        return new BlockId(block);
    }

    public static BlockId EmptyWorldGeneratedBlock(BlockPosition position)
    {
        return new BlockId(EmptyWorldGeneratedBlockPointer(
            position.X,
            position.Y,
            position.Z));
    }

    public static BlockId FlatTestGeneratedBlock(BlockPosition position, in NativeTerrainMaterialRules rules)
    {
        var nativeRules = rules;
        return new BlockId(FlatTestGeneratedBlockPointer(
            position.X,
            position.Y,
            position.Z,
            &nativeRules));
    }

    public static int ClearTerrainMatchingOverrides(BlockStore blocks, in NativeTerrainMaterialRules rules)
    {
        var nativeRules = rules;
        return ClearTerrainMatchingOverridesPointer(blocks.NativeHandle, &nativeRules);
    }

    public static int ClearEmptyWorldMatchingOverrides(BlockStore blocks)
    {
        return ClearEmptyWorldMatchingOverridesPointer(blocks.NativeHandle);
    }

    public static int ClearFlatTestMatchingOverrides(BlockStore blocks, in NativeTerrainMaterialRules rules)
    {
        var nativeRules = rules;
        return ClearFlatTestMatchingOverridesPointer(blocks.NativeHandle, &nativeRules);
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_TERRAIN_GENERATION_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativeTerrainGenerationLibrary).Assembly.Location;
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
