using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Server.Persistence.Players;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

namespace Octaryn.Server.Simulation.Players;

internal sealed unsafe class NativePlayerSimulation
{
    private const string LibraryName = "octaryn_server_player_simulation";

    private readonly BlockStore _blocks;
    private readonly IBlockAuthorityRules _blockRules;
    private readonly Func<BlockPosition, BlockId>? _generatedBlocks;

    private static readonly delegate* unmanaged[Cdecl]<float> s_spawnEyeHeight;
    private static readonly delegate* unmanaged[Cdecl]<NativeState*, int> s_defaultState;
    private static readonly delegate* unmanaged[Cdecl]<float, float, float, float, float, ushort, NativeState*, int> s_stateFromSave;
    private static readonly delegate* unmanaged[Cdecl]<NativeSaveState*, NativeSaveState*, uint> s_saveStateChanged;
    private static readonly delegate* unmanaged[Cdecl]<NativeState*, uint, IntPtr, delegate* unmanaged[Cdecl]<void*, int, int, int, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, void*, NativeSpawnAlignment*, int> s_alignSpawnWithBlockStore;
    private static readonly delegate* unmanaged[Cdecl]<NativeInput*, double, IntPtr, delegate* unmanaged[Cdecl]<void*, int, int, int, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, void*, NativeState*, int> s_moveWithBlockStore;
    private static readonly delegate* unmanaged[Cdecl]<NativeInput*, uint> s_hasInputIntent;
    private static readonly delegate* unmanaged[Cdecl]<byte*, NativeInputIntent*, int> s_readInputIntentFile;
    private static readonly delegate* unmanaged[Cdecl]<NativeState*, int> s_idle;

    static NativePlayerSimulation()
    {
        var library = NativeLibrary.Load(ResolveLibraryPath());
        s_spawnEyeHeight = (delegate* unmanaged[Cdecl]<float>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_spawn_eye_height");
        s_defaultState = (delegate* unmanaged[Cdecl]<NativeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_default_state");
        s_stateFromSave = (delegate* unmanaged[Cdecl]<float, float, float, float, float, ushort, NativeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_state_from_save");
        s_saveStateChanged = (delegate* unmanaged[Cdecl]<NativeSaveState*, NativeSaveState*, uint>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_save_state_changed");
        s_alignSpawnWithBlockStore = (delegate* unmanaged[Cdecl]<NativeState*, uint, IntPtr, delegate* unmanaged[Cdecl]<void*, int, int, int, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, void*, NativeSpawnAlignment*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_align_spawn_with_block_store");
        s_moveWithBlockStore = (delegate* unmanaged[Cdecl]<NativeInput*, double, IntPtr, delegate* unmanaged[Cdecl]<void*, int, int, int, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, void*, NativeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_move_with_block_store");
        s_hasInputIntent = (delegate* unmanaged[Cdecl]<NativeInput*, uint>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_has_input_intent");
        s_readInputIntentFile = (delegate* unmanaged[Cdecl]<byte*, NativeInputIntent*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_read_input_intent_file");
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

    public static float SpawnEyeHeight => s_spawnEyeHeight();

    public static PlayerState DefaultState()
    {
        var nativeState = default(NativeState);
        var result = s_defaultState(&nativeState);
        if (result != 0)
        {
            throw new InvalidOperationException("Native player default state failed.");
        }

        return ToPlayerState(nativeState);
    }

    public static bool TryCreateStateFromSave(PlayerSaveState saved, out PlayerState state)
    {
        var nativeState = default(NativeState);
        var result = s_stateFromSave(
            saved.X,
            saved.Y,
            saved.Z,
            saved.Pitch,
            saved.Yaw,
            saved.SelectedBlock.Value,
            &nativeState);
        state = result == 0 ? ToPlayerState(nativeState) : default;
        return result == 0;
    }

    public static bool SaveStateChanged(PlayerSaveState previous, PlayerSaveState current)
    {
        var nativePrevious = ToNativeSaveState(previous);
        var nativeCurrent = ToNativeSaveState(current);
        return s_saveStateChanged(&nativePrevious, &nativeCurrent) != 0;
    }

    public bool TryAlignSpawnToSurface(
        PlayerState state,
        bool loadedFromSave,
        out PlayerState alignedState,
        out bool adjusted,
        out int surfaceY,
        out BlockId surfaceBlock)
    {
        var nativeState = ToNativeState(state);
        var alignment = default(NativeSpawnAlignment);
        var handle = GCHandle.Alloc(this);
        try
        {
            var result = s_alignSpawnWithBlockStore(
                &nativeState,
                loadedFromSave ? 1u : 0u,
                _blocks.NativeHandle,
                &GetGeneratedBlock,
                &IsSolidBlock,
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
        adjusted = alignment.Adjusted != 0;
        surfaceY = alignment.SurfaceY;
        surfaceBlock = new BlockId(alignment.SurfaceBlock);
        return alignment.Aligned != 0;
    }

    public PlayerState Move(PlayerState state, HostInputSnapshot input, double deltaSeconds)
    {
        var nativeState = ToNativeState(state);
        var nativeInput = ToNativeInput(input);
        var handle = GCHandle.Alloc(this);
        try
        {
            var result = s_moveWithBlockStore(
                &nativeInput,
                deltaSeconds,
                _blocks.NativeHandle,
                &GetGeneratedBlock,
                &IsSolidBlock,
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

    public static bool HasInputIntent(HostInputSnapshot input)
    {
        var nativeInput = ToNativeInput(input);
        return s_hasInputIntent(&nativeInput) != 0;
    }

    public static int ReadInputIntentFile(string path, out NativeInputIntent intent)
    {
        intent = default;
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var nativeIntent = stackalloc NativeInputIntent[1];
            var result = s_readInputIntentFile((byte*)pathPointer, nativeIntent);
            intent = nativeIntent[0];
            return result;
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
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
    private static ushort GetGeneratedBlock(void* context, int x, int y, int z)
    {
        if (context is null)
        {
            return BlockId.Air.Value;
        }

        var handle = GCHandle.FromIntPtr((IntPtr)context);
        if (handle.Target is not NativePlayerSimulation simulation)
        {
            return BlockId.Air.Value;
        }

        var position = new BlockPosition(x, y, z);
        return (simulation._generatedBlocks?.Invoke(position) ?? BlockId.Air).Value;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint IsSolidBlock(void* context, ushort block)
    {
        if (context is null)
        {
            return 0;
        }

        var handle = GCHandle.FromIntPtr((IntPtr)context);
        return handle.Target is NativePlayerSimulation simulation &&
            simulation._blockRules.IsSolidBlock(new BlockId(block))
                ? 1u
                : 0u;
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

    private static NativeInput ToNativeInput(HostInputSnapshot input)
    {
        return new NativeInput(
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
    }

    private static NativeSaveState ToNativeSaveState(PlayerSaveState state)
    {
        return new NativeSaveState(
            state.X,
            state.Y,
            state.Z,
            state.Pitch,
            state.Yaw,
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
    internal readonly struct NativeInput(
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
    internal readonly struct NativeInputIntent(
        int version,
        ulong frameIndex,
        double deltaSeconds,
        NativeInput input)
    {
        public readonly int Version = version;
        public readonly ulong FrameIndex = frameIndex;
        public readonly double DeltaSeconds = deltaSeconds;
        public readonly NativeInput Input = input;

        public HostFrameSnapshot ToFrameSnapshot()
        {
            return new HostFrameSnapshot(
                new HostInputSnapshot(
                    HostInputSnapshot.VersionValue,
                    HostInputSnapshot.SizeValue,
                    Input.Flags,
                    Input.Controller,
                    Input.MoveX,
                    Input.MoveY,
                    Input.MoveZ,
                    Input.CameraX,
                    Input.CameraY,
                    Input.CameraZ,
                    Input.CameraPitch,
                    Input.CameraYaw,
                    Input.RelativeMouse),
                new HostFrameTimingSnapshot(
                    HostFrameTimingSnapshot.VersionValue,
                    HostFrameTimingSnapshot.SizeValue,
                    FrameIndex,
                    DeltaSeconds));
        }
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
    private struct NativeSaveState(
        float x,
        float y,
        float z,
        float pitch,
        float yaw,
        ushort selectedBlock)
    {
        public float X = x;
        public float Y = y;
        public float Z = z;
        public float Pitch = pitch;
        public float Yaw = yaw;
        public ushort SelectedBlock = selectedBlock;
        public ushort Reserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeSpawnAlignment
    {
        public uint Aligned;
        public uint Adjusted;
        public int SurfaceY;
        public ushort SurfaceBlock;
        public ushort Reserved;
    }
}
