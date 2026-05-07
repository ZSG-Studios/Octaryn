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
    private static readonly delegate* unmanaged[Cdecl]<NativeState*, uint, IntPtr> s_sessionCreate;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, void> s_sessionDestroy;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeState*, int> s_sessionState;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, uint> s_sessionLoadedFromSave;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, IntPtr, delegate* unmanaged[Cdecl]<void*, int, int, int, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, void*, NativeSpawnAlignment*, int> s_sessionAlignSpawnWithBlockStore;
    private static readonly delegate* unmanaged[Cdecl]<NativeInput*, double, IntPtr, delegate* unmanaged[Cdecl]<void*, int, int, int, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, void*, IntPtr, NativeTickResult*, int> s_sessionStepWithBlockStore;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, double, uint, NativePlayerSessionSaveResult*, int> s_sessionSaveDecision;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeSaveState*, int> s_sessionNoteSaved;
    private static readonly delegate* unmanaged[Cdecl]<byte*, uint, NativeInputProcessResult*, int> s_readProcessInputIntent;
    private static readonly delegate* unmanaged[Cdecl]<uint, byte*> s_inputProcessReasonName;
    private static readonly delegate* unmanaged[Cdecl]<uint, byte*> s_controlModeName;
    private static readonly delegate* unmanaged[Cdecl]<uint, uint> s_controlModeIsFly;

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
        s_sessionCreate = (delegate* unmanaged[Cdecl]<NativeState*, uint, IntPtr>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_session_create");
        s_sessionDestroy = (delegate* unmanaged[Cdecl]<IntPtr, void>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_session_destroy");
        s_sessionState = (delegate* unmanaged[Cdecl]<IntPtr, NativeState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_session_state");
        s_sessionLoadedFromSave = (delegate* unmanaged[Cdecl]<IntPtr, uint>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_session_loaded_from_save");
        s_sessionAlignSpawnWithBlockStore = (delegate* unmanaged[Cdecl]<IntPtr, IntPtr, delegate* unmanaged[Cdecl]<void*, int, int, int, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, void*, NativeSpawnAlignment*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_session_handle_align_spawn_with_block_store");
        s_sessionStepWithBlockStore = (delegate* unmanaged[Cdecl]<NativeInput*, double, IntPtr, delegate* unmanaged[Cdecl]<void*, int, int, int, ushort>, delegate* unmanaged[Cdecl]<void*, ushort, uint>, void*, IntPtr, NativeTickResult*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_session_handle_step_with_block_store");
        s_sessionSaveDecision = (delegate* unmanaged[Cdecl]<IntPtr, double, uint, NativePlayerSessionSaveResult*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_session_handle_save_decision");
        s_sessionNoteSaved = (delegate* unmanaged[Cdecl]<IntPtr, NativeSaveState*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_session_handle_note_saved");
        s_readProcessInputIntent = (delegate* unmanaged[Cdecl]<byte*, uint, NativeInputProcessResult*, int>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_read_process_input_intent");
        s_inputProcessReasonName = (delegate* unmanaged[Cdecl]<uint, byte*>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_input_process_reason_name");
        s_controlModeName = (delegate* unmanaged[Cdecl]<uint, byte*>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_control_mode_name");
        s_controlModeIsFly = (delegate* unmanaged[Cdecl]<uint, uint>)NativeLibrary.GetExport(
            library,
            "octaryn_server_player_control_mode_is_fly");
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

    public static IntPtr CreateSession(PlayerState state, bool loadedFromSave)
    {
        var nativeState = ToNativeState(state);
        var session = s_sessionCreate(&nativeState, loadedFromSave ? 1u : 0u);
        if (session == IntPtr.Zero)
        {
            throw new InvalidOperationException("Native player session creation failed.");
        }

        return session;
    }

    public static void DestroySession(IntPtr session)
    {
        if (session != IntPtr.Zero)
        {
            s_sessionDestroy(session);
        }
    }

    public static PlayerState StateFromSession(IntPtr session)
    {
        var nativeState = default(NativeState);
        var result = s_sessionState(session, &nativeState);
        if (result != 0)
        {
            throw new InvalidOperationException("Native player session state read failed.");
        }

        return ToPlayerState(nativeState);
    }

    public static bool SessionLoadedFromSave(IntPtr session)
    {
        return s_sessionLoadedFromSave(session) != 0;
    }

    public static NativePlayerSessionSaveResult SaveDecision(IntPtr session, double deltaSeconds, bool force)
    {
        var result = default(NativePlayerSessionSaveResult);
        var nativeResult = s_sessionSaveDecision(
            session,
            deltaSeconds,
            force ? 1u : 0u,
            &result);
        if (nativeResult != 0)
        {
            throw new InvalidOperationException("Native player session save decision failed.");
        }

        return result;
    }

    public static PlayerSaveState SaveStateFromSessionSaveResult(NativePlayerSessionSaveResult result)
    {
        return ToPlayerSaveState(result.SaveState);
    }

    public static void NoteSaved(IntPtr session, PlayerSaveState saveState)
    {
        var nativeSaveState = ToNativeSaveState(saveState);
        var result = s_sessionNoteSaved(session, &nativeSaveState);
        if (result != 0)
        {
            throw new InvalidOperationException("Native player session save note failed.");
        }
    }

    public bool TryAlignSpawnToSurface(
        IntPtr session,
        out PlayerState alignedState,
        out bool adjusted,
        out int surfaceY,
        out BlockId surfaceBlock)
    {
        var alignment = default(NativeSpawnAlignment);
        var handle = GCHandle.Alloc(this);
        try
        {
            var result = s_sessionAlignSpawnWithBlockStore(
                session,
                _blocks.NativeHandle,
                &GetGeneratedBlock,
                &IsSolidBlock,
                (void*)GCHandle.ToIntPtr(handle),
                &alignment);
            if (result != 0)
            {
                throw new InvalidOperationException("Native player session spawn alignment failed.");
            }
        }
        finally
        {
            handle.Free();
        }

        alignedState = StateFromSession(session);
        adjusted = alignment.Adjusted != 0;
        surfaceY = alignment.SurfaceY;
        surfaceBlock = new BlockId(alignment.SurfaceBlock);
        return alignment.Aligned != 0;
    }

    public PlayerState Step(IntPtr session, HostInputSnapshot input, double deltaSeconds, out NativeTickResult tickResult)
    {
        var nativeInput = ToNativeInput(input);
        var nativeTickResult = default(NativeTickResult);
        var handle = GCHandle.Alloc(this);
        try
        {
            var result = s_sessionStepWithBlockStore(
                &nativeInput,
                deltaSeconds,
                _blocks.NativeHandle,
                &GetGeneratedBlock,
                &IsSolidBlock,
                (void*)GCHandle.ToIntPtr(handle),
                session,
                &nativeTickResult);
            if (result != 0)
            {
                throw new InvalidOperationException("Native player session step failed.");
            }
        }
        finally
        {
            handle.Free();
        }

        tickResult = nativeTickResult;
        return StateFromSession(session);
    }

    public static int ReadProcessInputIntent(string path, bool allowTransientInvalid, out NativeInputProcessResult result)
    {
        result = default;
        var pathPointer = Marshal.StringToCoTaskMemUTF8(path);
        try
        {
            var nativeResult = stackalloc NativeInputProcessResult[1];
            var readResult = s_readProcessInputIntent(
                (byte*)pathPointer,
                allowTransientInvalid ? 1u : 0u,
                nativeResult);
            result = nativeResult[0];
            return readResult;
        }
        finally
        {
            Marshal.FreeCoTaskMem(pathPointer);
        }
    }

    public static string InputProcessReasonName(uint reason)
    {
        return Marshal.PtrToStringUTF8((IntPtr)s_inputProcessReasonName(reason)) ?? "intent_read_failed";
    }

    public static string ControlModeName(uint mode)
    {
        return Marshal.PtrToStringUTF8((IntPtr)s_controlModeName(mode)) ?? "walk";
    }

    public static bool IsFlyControlMode(uint mode)
    {
        return s_controlModeIsFly(mode) != 0;
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
            state.ControlMode,
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

    private static PlayerSaveState ToPlayerSaveState(NativeSaveState state)
    {
        return new PlayerSaveState(
            state.X,
            state.Y,
            state.Z,
            state.Pitch,
            state.Yaw,
            new BlockId(state.SelectedBlock));
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
            state.ControlMode,
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

}
