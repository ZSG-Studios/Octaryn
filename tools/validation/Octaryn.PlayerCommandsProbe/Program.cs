using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text.Json;
using Octaryn.Server;
using Octaryn.Server.Simulation.Players;
using Octaryn.Shared.Host;

internal static unsafe class Program
{
    private static delegate* unmanaged[Cdecl]<NativeInput*, double,
        delegate* unmanaged[Cdecl]<void*, int, int, int, uint>, void*, NativeState*, NativeTickResult*, int> s_step;
    private static string s_path = "";
    private static double s_time;
    private static ulong s_ticks;
    private static int s_jumpSteps;
    private static NativeState s_state;
    private static PlayerCommandQueue s_commands = new(() => s_time);

    private static int Main(string[] args)
    {
        if (args.Length == 2 && args[0] == "--player-ecs") return PlayerSimulationQualification.Run(args[1]);
        if (args.Length != 2) throw new ArgumentException("native player library, output directory required");
        Directory.CreateDirectory(args[1]);
        s_path = Path.Combine(args[1], "player_input.json");
        var library = NativeLibrary.Load(Path.GetFullPath(args[0]));
        s_step = (delegate* unmanaged[Cdecl]<NativeInput*, double,
            delegate* unmanaged[Cdecl]<void*, int, int, int, uint>, void*, NativeState*, NativeTickResult*, int>)
            NativeLibrary.GetExport(library, "octaryn_server_player_step");
        try
        {
            Reset();
            var worldBudget = new FixedStepBudget(() => s_time);
            Write(1, 1, 4, 1);
            Require(Drain() == 0, "client grants no initial simulation credit");
            Advance(1.0 / 60);
            Require(s_commands.Acknowledged == 1 && s_state.X > 0, "command moves real native body");
            var firstX = s_state.X;
            Write(1, 1, 4, 1);
            Advance(0.1);
            worldBudget.Accrue();
            var worldSteps = 0;
            while (worldBudget.CanStep) { worldBudget.Commit(); worldSteps++; }
            Require(worldSteps == 7, "world advances once on independent wall clock");
            Require(s_state.X == firstX && s_ticks == 1, "duplicates and idle do not replay held input");
            Write(2, 6, 4, 1);
            Require(Drain() == 6 && s_commands.Acknowledged == 7, "retained wall credit catches up delayed batch");
            worldBudget.Accrue();
            Require(!worldBudget.CanStep, "actor catchup does not step world again");
            Console.WriteLine($"commands duplicate=pass catchup=6 x={s_state.X:R}");
            Console.WriteLine($"commands independent_world=pass world_steps={worldSteps} actor_steps={s_ticks}");

            Reset();
            Write(1, 64, 4, 1);
            Advance(0.1);
            Require(s_ticks == 6 && s_commands.Acknowledged == 6, "burst cannot exceed wall budget");
            var burstX = s_state.X;
            for (var i = 0; i < 100; i++) { Write(1, 64, 4, 1); Drain(); }
            Require(s_state.X == burstX && s_ticks == 6, "duplicate flood cannot speed up body");
            Advance(0.3);
            Require(s_commands.Acknowledged == 64 && s_state.X == burstX, "stale backlog retires without replay");
            Require(s_ticks <= 14, "stall catchup bounded to eight steps");
            Write(65, 1, 4, 1);
            Advance(1.0 / 60);
            Require(s_commands.Acknowledged == 65 && s_state.X > burstX, "fresh command recovers after stale burst");
            Console.WriteLine($"commands burst=pass duplicate_flood=100 stale_ack=64 recovered_ack=65 ticks={s_ticks}");

            Reset();
            Write(2, 1, 4, 1);
            Advance(1.0 / 60);
            Require(s_commands.Acknowledged == 0 && s_state.X == 0, "gap does not acknowledge or move");
            Write(1, 2, 4, 1);
            Advance(1.0 / 60);
            Require(s_commands.Acknowledged == 1, "gap recovered in order within available credit");
            Console.WriteLine("commands gap=pass");

            Reset();
            s_state.Y = 3.8f;
            s_state.IsOnGround = 1;
            Write(1, 1, 1, 0);
            Advance(1.0 / 60);
            Require(s_state.JumpHeld != 0 && s_state.VelocityY > 0, "short jump press reaches native rollback state");
            Write(2, 1, 0, 0);
            Advance(1.0 / 60);
            Require(s_commands.Acknowledged == 2 && s_state.JumpHeld == 0, "short jump release preserved");
            Console.WriteLine($"commands short_tap=pass y={s_state.Y:R} vy={s_state.VelocityY:R} jumpHeld={s_state.JumpHeld}");

            Reset();
            s_state.Y = 3.8f;
            s_state.IsOnGround = 1;
            File.WriteAllText(s_path, JsonSerializer.Serialize(new {
                version = 2, commands = new[] { 1u, 0u }.Select((flags, i) => new {
                    frameIndex = (ulong)i + 1, flags, controller = 1,
                    moveX = 0, moveY = 0, moveZ = 0, cameraPitch = 0, cameraYaw = 0, relativeMouse = 1
                })
            }));
            s_commands.Read(s_path);
            Advance(2.0 / 60);
            Require(s_commands.Acknowledged == 2 && s_jumpSteps == 1 && s_state.VelocityY > 0,
                "press and release in the same overwritten mailbox batch both execute");
            Console.WriteLine("commands batched_short_tap=pass press_steps=1 released=1");

            Reset();
            for (ulong first = 1; first <= 257; first += 64) Write(first, 64, 4, 1);
            Advance(0.3);
            Require(s_commands.Acknowledged == 256 && s_state.X == 0,
                "outstanding cap rejects beyond 256; expired backlog coalesces without movement");
            Write(257, 1, 4, 1);
            Advance(1.0 / 60);
            Require(s_commands.Acknowledged == 257 && s_state.X > 0, "window recovers after bounded retirement");
            Console.WriteLine("commands outstanding_cap=256 expired_recovery=pass");

            Reset();
            for (ulong sequence = 1; sequence <= 240; sequence++)
            {
                Write(sequence, 1, 4, 1);
                Advance(1.0 / 120);
            }
            Require(s_ticks <= 120 && s_state.X <= 20.001f,
                "120 Hz new commands cannot accelerate the 60 Hz authoritative body");
            Console.WriteLine($"commands double_client_rate=pass wall_seconds=2 actor_steps={s_ticks} x={s_state.X:R} ack={s_commands.Acknowledged}");

            Reset();
            Write(1, 65, 4, 1);
            Advance(1.0 / 60);
            Require(s_commands.Acknowledged == 0, "oversized batch rejected");
            s_commands.Reset();
            Require(s_commands.Acknowledged == 0 && Drain() == 0, "session resets sequence and credit");
            Require(ChunkStreamProcessBridge.EvaluateCommandDependency(257, 0) ==
                ChunkStreamProcessBridge.CommandDependency.Reject, "far-future action rejected");
            Require(ChunkStreamProcessBridge.EvaluateCommandDependency(1, 0.5) ==
                ChunkStreamProcessBridge.CommandDependency.Reject, "action wait bounded");
            Console.WriteLine("commands bounds=pass reset=pass action_dependency=pass");
            Reset();
            PlayerTransportChecks.Run(frame => StepNative(in frame));
            return 0;
        }
        finally { NativeLibrary.Free(library); }
    }

    private static void Reset()
    {
        s_time = 1;
        s_ticks = 0;
        s_jumpSteps = 0;
        s_state = new NativeState(0, 80, 0, 0, 0, 0, 0, 0, 0, 1, 1);
        s_commands = new PlayerCommandQueue(() => s_time);
    }

    private static void Write(ulong first, int count, uint flags, float moveX)
    {
        var commands = Enumerable.Range(0, count).Select(i => new {
            frameIndex = first + (ulong)i, flags, controller = 1,
            moveX, moveY = 0, moveZ = 0, cameraPitch = 0, cameraYaw = 0, relativeMouse = 1,
            deltaSeconds = 1000000 // Deliberately ignored: cannot buy simulation time.
        });
        File.WriteAllText(s_path, JsonSerializer.Serialize(new { version = 2, commands }));
        s_commands.Read(s_path);
    }

    private static void Advance(double seconds)
    {
        s_time += seconds;
        s_commands.Accrue();
        Drain();
    }

    private static int Drain()
    {
        var steps = 0;
        while (steps < 8 && s_commands.TrySelect(s_ticks + 1, out var frame))
        {
            StepNative(in frame);
            s_commands.Commit(in frame);
            s_ticks++;
            steps++;
        }
        return steps;
    }

    private static void StepNative(in HostFrameSnapshot frame)
    {
        var input = frame.Input;
        var native = new NativeInput(input.Flags, input.Controller, input.MoveX, input.MoveY,
            input.MoveZ, 0, 0, 0, input.CameraPitch, input.CameraYaw, input.RelativeMouse);
        var state = s_state;
        var result = default(NativeTickResult);
        Require(s_step(&native, frame.Timing.DeltaSeconds, &Block, null, &state, &result) == 0, "native step");
        s_state = state;
        if (state.JumpHeld != 0) s_jumpSteps++;
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static uint Block(void* context, int x, int y, int z) => y < 0 ? 0x10001u : 0u;
    private static void Require(bool condition, string name)
    {
        if (!condition) throw new InvalidOperationException(name);
    }
}
