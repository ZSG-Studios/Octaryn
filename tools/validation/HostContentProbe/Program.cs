using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.Host.Api;

internal static unsafe partial class Program
{
    private static HostContentApiTable* s_table;
    private static int s_mode, s_calls;

    private static int Main(string[] args)
    {
        var root = Path.Combine(Path.GetTempPath(), "octaryn-content-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(Path.Combine(root, "Data"));
            File.WriteAllText(Path.Combine(root, "Data", "status.txt"), "ready", Encoding.ASCII);
            File.WriteAllBytes(Path.Combine(root, "Data", "empty.bin"), []);
            using (var large = File.Create(Path.Combine(root, "Data", "large.bin")))
                large.SetLength(IHostContentApi.MaximumReadBytes + 1L);
            using (var limit = File.Create(Path.Combine(root, "Data", "limit.bin")))
                limit.SetLength(IHostContentApi.MaximumReadBytes);
            VerifyBundleRoot(root);
            VerifyManaged(root);
            VerifyManifest();
            VerifyNative(root);
            VerifyScene(root);
            VerifyUiSampling(root);
            if (args.Length > 0) VerifyLiveScene(args);
            Console.WriteLine("host_content_probe=passed declared_reads=1 scope=1 gating=1 traversal=1 size_limit=1 immutable_reads=1 native_layout=1 native_bounds=1 native_absence=1 native_size_change=1");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
        finally
        {
            if (s_table is not null) NativeMemory.Free(s_table);
            var resolved = Path.GetFullPath(root);
            var temporary = Path.TrimEndingDirectorySeparator(Path.GetFullPath(Path.GetTempPath())) + Path.DirectorySeparatorChar;
            if (resolved.StartsWith(temporary, StringComparison.OrdinalIgnoreCase) && Directory.Exists(resolved))
                Directory.Delete(resolved, recursive: true);
        }
    }

    private static void VerifyManaged(string root)
    {
        var manifest = Manifest();
        IHostApiProvider host = new Host();
        var enabled = HostModuleContext.Create(manifest, new Host(), host, root);
        var api = enabled.Content ?? throw new InvalidOperationException("Requested content API unavailable.");
        Require(HostApiAllowlist.IsAllowed(HostApiIds.Content), "content API absent from allowlist");
        Require(api.TryRead("octaryn.basegame.status", out var first, out var error) &&
            Encoding.ASCII.GetString(first.Span) == "ready" && error.Length == 0, "declared read failed");
        File.WriteAllText(Path.Combine(root, "Data", "status.txt"), "changed", Encoding.ASCII);
        Require(api.TryRead("octaryn.basegame.status", out var second, out _) &&
            Encoding.ASCII.GetString(first.Span) == "ready" && Encoding.ASCII.GetString(second.Span) == "changed", "read results changed or content was cached");
        Require(api.TryRead("octaryn.basegame.empty", out var empty, out _) && empty.Length == 0, "empty file rejected");
        Require(api.TryRead("octaryn.basegame.limit", out var limit, out _) && limit.Length == IHostContentApi.MaximumReadBytes, "exact read limit rejected");
        foreach (var denied in new[] { "other.module.status", "octaryn.basegame.unknown", "octaryn.basegame.escape", "octaryn.basegame.trimmed", "octaryn.basegame.absolute", "octaryn.basegame.large", "Data/status.txt" })
            Require(!api.TryRead(denied, out var data, out var reason) && data.Length == 0 && reason.Length > 0, "unexpected access: " + denied);
        var unrequested = HostModuleContext.Create(manifest with { RequestedHostApis = [] }, new Host(), host, root);
        Require(unrequested.Content is null, "unrequested capability granted");
        var absent = HostModuleContext.Create(manifest, new Host(), null, root);
        Require(absent.Content is null, "missing provider granted API");
        File.Delete(Path.Combine(root, "Data", "status.txt"));
        Require(!api.TryRead("octaryn.basegame.status", out _, out _), "deleted data still available");
        File.WriteAllText(Path.Combine(root, "Data", "status.txt"), "ready", Encoding.ASCII);
    }

    private static void VerifyNative(string root)
    {
        Require(sizeof(HostContentApiTable) == 24 && Marshal.OffsetOf<HostContentApiTable>(nameof(HostContentApiTable.ReadData)) == 8 &&
            Marshal.OffsetOf<HostContentApiTable>(nameof(HostContentApiTable.MaximumReadBytes)) == 16, "native layout mismatch");
        var provider = new NativeHostApiProvider(&Query);
        Require(provider.GetContentApi(Manifest(), root)!.TryRead("octaryn.basegame.status", out _, out _), "native absence did not retain managed host content");
        s_table = (HostContentApiTable*)NativeMemory.AllocZeroed((nuint)sizeof(HostContentApiTable));
        *s_table = new HostContentApiTable { Version = 1, Size = 24, ReadData = &Read, MaximumReadBytes = IHostContentApi.MaximumReadBytes };
        var api = provider.GetContentApi(Manifest(), root)!;
        Require(api.TryRead("octaryn.basegame.status", out var data, out _) && Encoding.ASCII.GetString(data.Span) == "ready", "native copy failed");
        var calls = s_calls;
        Require(!api.TryRead("other.module.status", out _, out _) && s_calls == calls, "native read bypassed declaration scope");
        s_mode = 1;
        Require(!api.TryRead("octaryn.basegame.status", out _, out _) && s_calls == calls + 1, "oversized native allocation accepted");
        s_mode = 2;
        Require(!api.TryRead("octaryn.basegame.status", out _, out _), "native size change accepted");
        s_mode = 3;
        Require(!api.TryRead("octaryn.basegame.status", out _, out _), "native callback error accepted");
        s_mode = 0;
        s_table->Size = 8;
        Require(provider.GetContentApi(Manifest(), root) is null, "short native table accepted");
        s_table->Size = 24;
        s_table->ReadData = null;
        Require(provider.GetContentApi(Manifest(), root) is null, "absent native callback accepted");
        s_table->ReadData = &Read;
        s_table->MaximumReadBytes = IHostContentApi.MaximumReadBytes + 1u;
        Require(provider.GetContentApi(Manifest(), root) is null, "unbounded native table accepted");
    }

    private static void VerifyManifest()
    {
        var system = new ScheduledSystemDeclaration("octaryn.basegame.tick", HostWorkPhase.Gameplay,
            HostScheduleIds.FrameOrTickOwner, [new(HostApiIds.Content, ScheduledAccessMode.Read)],
            [], [], [], HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier,
            HostScheduleIds.FrameOrTickEndBarrier);
        var manifest = Manifest() with
        {
            ContentDeclarations = [new("octaryn.basegame.status", "data", "Data/status.txt")],
            Schedule = new([system])
        };
        var valid = GameModuleValidator.Validate(manifest);
        Require(valid.IsValid, "valid data/content read declaration rejected: " + string.Join(';', valid.Issues.Select(issue => issue.Message)));
        var denied = GameModuleValidator.Validate(manifest with { RequestedHostApis = [] });
        Require(denied.Issues.Any(issue => issue.Code == "module.schedule.content.read.required"), "scheduled read without capability request accepted");
        var writer = GameModuleValidator.Validate(manifest with
        {
            Schedule = new([system with { Reads = [], Writes = [new(HostApiIds.Content, ScheduledAccessMode.Write)] }])
        });
        Require(writer.Issues.Any(issue => issue.Code == "module.schedule.write.host_resource.invalid"), "read-only content write accepted");
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static void* Query(uint id, uint version) => id == HostApiTableIds.Content && version == 1 ? s_table : null;

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int Read(byte* module, byte* content, byte* buffer, uint capacity, uint* bytes)
    {
        ++s_calls;
        if (Marshal.PtrToStringUTF8((nint)module) != "octaryn.basegame" ||
            Marshal.PtrToStringUTF8((nint)content) != "octaryn.basegame.status" || s_mode == 3) return -1;
        *bytes = s_mode == 1 ? IHostContentApi.MaximumReadBytes + 1u : 5u;
        if (buffer is null) return 0;
        if (s_mode == 2) { *bytes = 4; return 0; }
        if (capacity < 5) return 1;
        "ready"u8.CopyTo(new Span<byte>(buffer, 5));
        return 0;
    }

    private static GameModuleManifest Manifest() => new("octaryn.basegame", "Octaryn Basegame", "0.1.0", "0.1.0", [], [HostApiIds.Content], [], [], [], [],
        [new("octaryn.basegame.status", "rule", "Data/status.txt"), new("octaryn.basegame.empty", "rule", "Data/empty.bin"),
         new("octaryn.basegame.large", "rule", "Data/large.bin"), new("octaryn.basegame.escape", "rule", "Data/../outside.txt"),
         new("octaryn.basegame.limit", "rule", "Data/limit.bin"), new("octaryn.basegame.trimmed", "rule", "Data/.. /outside.txt"),
         new("octaryn.basegame.absolute", "rule", "C:/outside.txt"), new("other.module.status", "rule", "Data/status.txt")],
        [], new GameModuleScheduleDeclaration([]), new("0.1.0", "0.1.0", "octaryn.basegame.save.v0", false));

    private static void Require(bool condition, string reason)
    {
        if (!condition) throw new InvalidOperationException(reason);
    }

    private sealed class Host : IHostApiProvider, IHostCommandSink
    {
        public bool Enqueue(HostCommand command) => false;
        public IHostTimeApi? GetTimeApi() => null;
        public IHostDiagnosticsApi? GetDiagnosticsApi() => null;
        public IHostPhysicsApi? GetPhysicsApi() => null;
        public IHostWorldApi? GetWorldApi() => null;
        public IHostPlayerApi? GetPlayerApi() => null;
        public IHostEcsApi? GetEcsApi() => null;
        public IHostInputApi? GetInputApi() => null;
        public IHostSchedulingApi? GetSchedulingApi() => null;
        public IHostAudioApi? GetAudioApi() => null;
        public IHostUiApi? GetUiApi() => null;
        public IHostReplicationApi? GetReplicationApi() => null;
    }
}
