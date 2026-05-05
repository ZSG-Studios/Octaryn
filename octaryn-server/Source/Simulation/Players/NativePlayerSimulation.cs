using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.Simulation.Players;

internal sealed unsafe class NativePlayerSimulation
{
    public const float SpawnEyeHeight = 2.72f;

    private const string LibraryName = "octaryn_server_player_simulation";
    private const uint SolidBlockFlag = 1u << 16;

    private readonly BlockStore _blocks;
    private readonly IBlockAuthorityRules _blockRules;
    private readonly Func<BlockPosition, BlockId>? _generatedBlocks;

    private static readonly delegate* unmanaged[Cdecl]<NativeState*, uint, delegate* unmanaged[Cdecl]<void*, int, int, int, uint>, void*, NativeSpawnAlignment*, int> s_alignSpawn;
    private static readonly delegate* unmanaged[Cdecl]<NativeInput*, double, delegate* unmanaged[Cdecl]<void*, int, int, int, uint>, void*, NativeState*, int> s_move;
    private static readonly delegate* unmanaged[Cdecl]<NativeState*, int> s_idle;

    static NativePlayerSimulation()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        s_alignSpawn = (delegate* unmanaged[Cdecl]<NativeState*, uint, delegate* unmanaged[Cdecl]<void*, int, int, int, uint>, void*, NativeSpawnAlignment*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_align_spawn");
        s_move = (delegate* unmanaged[Cdecl]<NativeInput*, double, delegate* unmanaged[Cdecl]<void*, int, int, int, uint>, void*, NativeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_move");
        s_idle = (delegate* unmanaged[Cdecl]<NativeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_idle");
    }

    public NativePlayerSimulation(
        BlockStore blocks,
        IBlockAuthorityRules blockRules,
        Func<BlockPosition, BlockId>? generatedBlocks = null)
    {
        _blocks = blocks;
        _blockRules = blockRules;
        _generatedBlocks = generatedBlocks;
    }

    public bool TryAlignSpawnToSurface(
        PlayerState state,
        bool loadedFromSave,
        out PlayerState alignedState,
        out int surfaceY,
        out BlockId surfaceBlock)
    {
        var nativeState = ToNativeState(state);
        var alignment = default(NativeSpawnAlignment);
        var handle = GCHandle.Alloc(this);
        try
        {
            var result = s_alignSpawn(
                &nativeState,
                loadedFromSave ? 1u : 0u,
                &GetBlockInfo,
                (void*)GCHandle.ToIntPtr(handle),
                &alignment);
            if (result != 0)
            {
                throw new InvalidOperationException("Native player spawn alignment failed.");
            }
        }
        finally
        {
            handle.Free();
        }

        alignedState = ToPlayerState(nativeState);
        surfaceY = alignment.SurfaceY;
        surfaceBlock = new BlockId(alignment.SurfaceBlock);
        return alignment.Aligned != 0;
    }

    public PlayerState Move(PlayerState state, HostInputSnapshot input, double deltaSeconds)
    {
        var nativeState = ToNativeState(state);
        var nativeInput = new NativeInput(
            input.Flags,
            input.Controller,
            input.MoveX,
            input.MoveY,
            input.MoveZ,
            input.CameraX,
            input.CameraY,
            input.CameraZ,
            input.CameraPitch,
            input.CameraYaw,
            input.RelativeMouse);
        var handle = GCHandle.Alloc(this);
        try
        {
            var result = s_move(
                &nativeInput,
                deltaSeconds,
                &GetBlockInfo,
                (void*)GCHandle.ToIntPtr(handle),
                &nativeState);
            if (result != 0)
            {
                throw new InvalidOperationException("Native player movement failed.");
            }
        }
        finally
        {
            handle.Free();
        }

        return ToPlayerState(nativeState);
    }

    public PlayerState Idle(PlayerState state)
    {
        var nativeState = ToNativeState(state);
        var result = s_idle(&nativeState);
        if (result != 0)
        {
            throw new InvalidOperationException("Native player idle update failed.");
        }

        return ToPlayerState(nativeState);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint GetBlockInfo(void* context, int x, int y, int z)
    {
        if (context is null)
        {
            return 0;
        }

        var handle = GCHandle.FromIntPtr((IntPtr)context);
        if (handle.Target is not NativePlayerSimulation simulation)
        {
            return 0;
        }

        var position = new BlockPosition(x, y, z);
        var block = simulation._blocks.TryGetBlock(position, out var overrideBlock)
            ? overrideBlock
            : simulation._generatedBlocks?.Invoke(position) ?? BlockId.Air;
        var info = (uint)block.Value;
        if (simulation._blockRules.IsSolidBlock(block))
        {
            info |= SolidBlockFlag;
        }

        return info;
    }

    private static NativeState ToNativeState(PlayerState state)
    {
        return new NativeState(
            state.X,
            state.Y,
            state.Z,
            state.Pitch,
            state.Yaw,
            state.VelocityX,
            state.VelocityY,
            state.VelocityZ,
            state.IsOnGround ? 1u : 0u,
            (uint)state.ControlMode,
            state.SelectedBlock.Value);
    }

    private static PlayerState ToPlayerState(NativeState state)
    {
        return new PlayerState(
            state.X,
            state.Y,
            state.Z,
            state.Pitch,
            state.Yaw,
            state.VelocityX,
            state.VelocityY,
            state.VelocityZ,
            state.IsOnGround != 0,
            (PlayerControlMode)state.ControlMode,
            new BlockId(state.SelectedBlock));
    }

    private static string ResolveLibraryPath()
    {
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_PLAYER_SIMULATION_LIBRARY");
        if (!string.IsNullOrWhiteSpace(explicitPath))
        {
            return explicitPath;
        }

        var fileName = RuntimeInformation.IsOSPlatform(OSPlatform.Windows)
            ? $"{LibraryName}.dll"
            : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)
                ? $"lib{LibraryName}.dylib"
                : $"lib{LibraryName}.so";
        var assemblyPath = typeof(NativePlayerSimulation).Assembly.Location;
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

    [StructLayout(LayoutKind.Sequential)]
    private readonly struct NativeInput(
        uint flags,
        uint controller,
        float moveX,
        float moveY,
        float moveZ,
        float cameraX,
        float cameraY,
        float cameraZ,
        float cameraPitch,
        float cameraYaw,
        int relativeMouse)
    {
        public readonly uint Flags = flags;
        public readonly uint Controller = controller;
        public readonly float MoveX = moveX;
        public readonly float MoveY = moveY;
        public readonly float MoveZ = moveZ;
        public readonly float CameraX = cameraX;
        public readonly float CameraY = cameraY;
        public readonly float CameraZ = cameraZ;
        public readonly float CameraPitch = cameraPitch;
        public readonly float CameraYaw = cameraYaw;
        public readonly int RelativeMouse = relativeMouse;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeState(
        float x,
        float y,
        float z,
        float pitch,
        float yaw,
        float velocityX,
        float velocityY,
        float velocityZ,
        uint isOnGround,
        uint controlMode,
        ushort selectedBlock)
    {
        public float X = x;
        public float Y = y;
        public float Z = z;
        public float Pitch = pitch;
        public float Yaw = yaw;
        public float VelocityX = velocityX;
        public float VelocityY = velocityY;
        public float VelocityZ = velocityZ;
        public uint IsOnGround = isOnGround;
        public uint ControlMode = controlMode;
        public ushort SelectedBlock = selectedBlock;
        public ushort Reserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeSpawnAlignment
    {
        public uint Aligned;
        public int SurfaceY;
        public ushort SurfaceBlock;
        public ushort Reserved;
    }
}
