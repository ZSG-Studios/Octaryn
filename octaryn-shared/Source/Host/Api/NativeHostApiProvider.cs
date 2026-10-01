using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace Octaryn.Shared.Host.Api;

// Projects native query results from octaryn_host_api_query_fn into the safe
// module-facing interfaces.
internal unsafe sealed partial class NativeHostApiProvider : IHostApiProvider
{
    private readonly delegate* unmanaged[Cdecl]<uint, uint, void*> _query;

    public NativeHostApiProvider(delegate* unmanaged[Cdecl]<uint, uint, void*> query)
    {
        _query = query;
    }

    public bool IsValid => _query is not null;

    public IHostTimeApi? GetTimeApi()
    {
        if (_query is null)
        {
            return null;
        }

        var table = (HostTimeApiTable*)_query(HostApiTableIds.Time, HostApiTableIds.TimeVersion);
        if (table is null ||
            table->Version < HostApiTableIds.TimeVersion ||
            table->Size < sizeof(HostTimeApiTable) ||
            table->NowSeconds is null ||
            table->TickId is null ||
            table->TickRate is null)
        {
            return null;
        }

        return new NativeTimeApi(table);
    }

    public IHostDiagnosticsApi? GetDiagnosticsApi()
    {
        if (_query is null)
        {
            return null;
        }

        var table = (HostDiagnosticsApiTable*)_query(HostApiTableIds.Diagnostics, HostApiTableIds.DiagnosticsVersion);
        if (table is null ||
            table->Version < HostApiTableIds.DiagnosticsVersion ||
            table->Size < sizeof(HostDiagnosticsApiTable) ||
            table->LogWrite is null)
        {
            return null;
        }

        return new NativeDiagnosticsApi(table);
    }

    public IHostWorldApi? GetWorldApi()
    {
        if (_query is null)
        {
            return null;
        }

        var table = (HostWorldApiTable*)_query(HostApiTableIds.World, HostApiTableIds.WorldVersion);
        if (table is null ||
            table->Version < HostApiTableIds.WorldVersion ||
            table->Size < sizeof(HostWorldApiTable) ||
            table->SpawnPose is null ||
            table->TriangleCount is null ||
            table->IsActive is null)
        {
            return null;
        }

        return new NativeWorldApi(table);
    }

    // The native host bridge keeps player authority in the managed host layer;
    // no native player table exists yet, so the API reports unavailable.
    public IHostPlayerApi? GetPlayerApi()
    {
        return null;
    }

    public IHostEcsApi? GetEcsApi()
    {
        return null;
    }

    public IHostInputApi? GetInputApi()
    {
        if (_query is null)
        {
            return null;
        }

        var table = (HostInputApiTable*)_query(HostApiTableIds.Input, HostApiTableIds.InputVersion);
        if (table is null ||
            table->Version < HostApiTableIds.InputVersion ||
            table->Size < sizeof(HostInputApiTable) ||
            table->PollInput is null)
        {
            return null;
        }

        return new NativeInputApi(table);
    }

    public IHostSchedulingApi? GetSchedulingApi()
    {
        if (_query is null)
        {
            return null;
        }

        var table = (HostSchedulingApiTable*)_query(HostApiTableIds.Scheduling, HostApiTableIds.SchedulingVersion);
        if (table is null ||
            table->Version < HostApiTableIds.SchedulingVersion ||
            table->Size < sizeof(HostSchedulingApiTable) ||
            table->SubmitWork is null)
        {
            return null;
        }

        return new NativeSchedulingApi(table);
    }

    public IHostAudioApi? GetAudioApi()
    {
        if (_query is null)
        {
            return null;
        }

        var table = (HostAudioApiTable*)_query(HostApiTableIds.Audio, HostApiTableIds.AudioVersion);
        if (table is null ||
            table->Version < HostApiTableIds.AudioVersion ||
            table->Size < sizeof(HostAudioApiTable) ||
            table->PlayActionSound is null)
        {
            return null;
        }

        return new NativeAudioApi(table);
    }

    public IHostUiApi? GetUiApi()
    {
        if (_query is null)
        {
            return null;
        }

        var table = (HostUiApiTable*)_query(HostApiTableIds.Ui, HostApiTableIds.UiVersion);
        if (table is null ||
            table->Version < HostApiTableIds.UiVersion ||
            table->Size < sizeof(HostUiApiTable) ||
            table->ShowNotification is null ||
            table->PollUiAction is null)
        {
            return null;
        }

        return new NativeUiApi(table);
    }

    public IHostReplicationApi? GetReplicationApi()
    {
        if (_query is null)
        {
            return null;
        }

        var table = (HostReplicationApiTable*)_query(HostApiTableIds.Replication, HostApiTableIds.ReplicationVersion);
        if (table is null ||
            table->Version < HostApiTableIds.ReplicationVersion ||
            table->Size < sizeof(HostReplicationApiTable) || table->AvailableChangeCapacity is null ||
            table->AvailableWorldItemCapacity is null || table->PublishWorldItem is null ||
            table->PublishChange is null ||
            table->SendMessage is null)
        {
            return null;
        }

        return new NativeReplicationApi(table);
    }

    private sealed class NativeTimeApi : IHostTimeApi
    {
        private readonly HostTimeApiTable* _table;

        public NativeTimeApi(HostTimeApiTable* table)
        {
            _table = table;
        }

        public double NowSeconds => _table->NowSeconds();

        public ulong TickId => _table->TickId();

        public double TickRate => _table->TickRate();
    }

    private sealed class NativeDiagnosticsApi : IHostDiagnosticsApi
    {
        private readonly HostDiagnosticsApiTable* _table;

        public NativeDiagnosticsApi(HostDiagnosticsApiTable* table)
        {
            _table = table;
        }

        public void Write(HostLogLevel level, string message)
        {
            var bytes = Encoding.UTF8.GetBytes(message);
            fixed (byte* text = bytes)
            {
                _table->LogWrite((uint)level, text);
            }
        }
    }

    private sealed class NativeWorldApi : IHostWorldApi
    {
        private readonly HostWorldApiTable* _table;

        public NativeWorldApi(HostWorldApiTable* table)
        {
            _table = table;
        }

        public bool IsActive => _table->IsActive() != 0;

        public ulong TriangleCount => _table->TriangleCount();

        public bool TryGetSpawnPose(out HostSpawnPose pose)
        {
            var nativePose = default(HostSpawnPoseNative);
            if (_table->SpawnPose(&nativePose) != 0 || nativePose.Valid == 0)
            {
                pose = default;
                return false;
            }

            pose = new HostSpawnPose(
                nativePose.X, nativePose.Y, nativePose.Z,
                nativePose.Yaw, nativePose.Pitch);
            return true;
        }
    }

    private sealed class NativeInputApi : IHostInputApi
    {
        private readonly HostInputApiTable* _table;

        public NativeInputApi(HostInputApiTable* table)
        {
            _table = table;
        }

        public bool TryPollInput(out HostInputState input)
        {
            var snapshot = new HostInputSnapshot(HostInputSnapshot.VersionValue, HostInputSnapshot.SizeValue);
            if (_table->PollInput(&snapshot) != 0)
            {
                input = default;
                return false;
            }

            input = new HostInputState(
                snapshot.Flags,
                snapshot.Controller,
                snapshot.MoveX,
                snapshot.MoveY,
                snapshot.MoveZ,
                snapshot.CameraPitch,
                snapshot.CameraYaw);
            return true;
        }
    }

    private sealed class NativeSchedulingApi : IHostSchedulingApi
    {
        private readonly HostSchedulingApiTable* _table;

        public NativeSchedulingApi(HostSchedulingApiTable* table)
        {
            _table = table;
        }

        public void RunOnMainThread(string jobId, Action work)
        {
            Submit(0u, work);
        }

        public void RunOnWorker(string jobId, Action work)
        {
            Submit(1u, work);
        }

        private void Submit(uint worker, Action work)
        {
            var handle = GCHandle.Alloc(work);
            var result = _table->SubmitWork(
                worker,
                &ExecuteWork,
                (void*)GCHandle.ToIntPtr(handle));
            if (result != 0)
            {
                handle.Free();
            }
        }

        [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
        private static void ExecuteWork(void* user)
        {
            var handle = GCHandle.FromIntPtr((IntPtr)user);
            try
            {
                ((Action)handle.Target!)();
            }
            finally
            {
                handle.Free();
            }
        }
    }

    private sealed class NativeAudioApi : IHostAudioApi
    {
        private readonly HostAudioApiTable* _table;

        public NativeAudioApi(HostAudioApiTable* table)
        {
            _table = table;
        }

        public bool PlayActionSound(ulong assetIdHash, float volume, float positionX, float positionY, float positionZ)
        {
            return _table->PlayActionSound(assetIdHash, volume, positionX, positionY, positionZ) == 0;
        }
    }

    private sealed class NativeUiApi : IHostUiApi
    {
        private readonly HostUiApiTable* _table;

        public NativeUiApi(HostUiApiTable* table)
        {
            _table = table;
        }

        public bool ShowNotification(string text)
        {
            var bytes = Encoding.UTF8.GetBytes(text + '\0');
            fixed (byte* buffer = bytes)
            {
                return _table->ShowNotification(buffer) == 0;
            }
        }

        public bool TryPollAction(out string actionId)
        {
            var buffer = stackalloc byte[256];
            if (_table->PollUiAction(buffer, 256u) != 0)
            {
                actionId = string.Empty;
                return false;
            }

            actionId = Encoding.UTF8.GetString(buffer, LengthOf(buffer, 256));
            return true;
        }

        private static int LengthOf(byte* buffer, int capacity)
        {
            var length = 0;
            while (length < capacity && buffer[length] != 0)
            {
                length++;
            }

            return length;
        }
    }

    private sealed class NativeReplicationApi : IHostReplicationApi
    {
        private readonly HostReplicationApiTable* _table;
        public int AvailableChangeCapacity => Math.Max(0, _table->AvailableChangeCapacity());
        public int AvailableWorldItemCapacity => Math.Max(0, _table->AvailableWorldItemCapacity());
        public bool PublishWorldItem(in HostWorldItemPose pose)
        {
            var copy = pose;
            return _table->PublishWorldItem(&copy) == 0;
        }

        public NativeReplicationApi(HostReplicationApiTable* table)
        {
            _table = table;
        }

        public bool PublishChange(uint changeKind, ulong replicationId, ulong payload0, ulong payload1)
        {
            var change = new Networking.ReplicationChange(changeKind, replicationId, payload0, payload1);
            return _table->PublishChange(&change) == 0;
        }

        public int SendMessage(ulong replicationId, ReadOnlySpan<byte> payload)
        {
            fixed (byte* data = payload)
            {
                return _table->SendMessage(replicationId, data, (uint)payload.Length);
            }
        }
    }
}
