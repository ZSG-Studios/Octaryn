using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Server.World.MapWorld;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Simulation.Players;

// Map-mode session movement: the native session keeps its state/save
// bookkeeping while the trampolines below forward movement to the loaded
// GLB map world.
internal sealed unsafe partial class NativePlayerSimulation
{
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, delegate* unmanaged[Cdecl]<void*, NativeState*, int>, void*, int> s_sessionAlignSpawnWithMap;
    private static readonly delegate* unmanaged[Cdecl]<IntPtr, NativeInput*, double, delegate* unmanaged[Cdecl]<void*, NativeInput*, double, NativeState*, NativeTickResult*, int>, void*, NativeTickResult*, int> s_sessionStepWithMap;

    public static PlayerState SpawnFromMap(IntPtr mapWorld, IntPtr session)
    {
        var result = s_sessionAlignSpawnWithMap(session, &MapWorldSpawnTrampoline, (void*)mapWorld);
        if (result != 0)
        {
            throw new InvalidOperationException("Native player session map spawn failed.");
        }

        return StateFromSession(session);
    }

    public PlayerState StepWithMap(IntPtr mapWorld, IntPtr session, HostInputSnapshot input, double deltaSeconds, out NativeTickResult tickResult)
    {
        var nativeInput = ToNativeInput(input);
        var nativeTickResult = default(NativeTickResult);
        var result = s_sessionStepWithMap(
            session,
            &nativeInput,
            deltaSeconds,
            &MapWorldStepTrampoline,
            (void*)mapWorld,
            &nativeTickResult);
        if (result != 0)
        {
            throw new InvalidOperationException("Native player session map step failed.");
        }

        tickResult = nativeTickResult;
        return StateFromSession(session);
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int MapWorldSpawnTrampoline(void* context, NativeState* state) =>
        NativeMapWorld.SpawnInto((IntPtr)context, state);

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int MapWorldStepTrampoline(void* context, NativeInput* input, double deltaSeconds, NativeState* state, NativeTickResult* result) =>
        NativeMapWorld.StepInto((IntPtr)context, input, deltaSeconds, state, result);
}
