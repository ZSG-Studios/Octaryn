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
            table->Size < 16 ||
            table->PlayActionSound is null)
        {
            return null;
        }

        return table->Version>=2 && table->Size>=sizeof(HostAudioApiTable) && table->RegisterPcm16 is not null && table->PlayClip is not null && table->StopVoice is not null && table->ReleaseClip is not null && table->QueryVoice is not null ? new NativePcmAudioApi(table) : new NativeAudioApi(table);
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
            table->PollUiAction is null || table->PresentScreen is null || table->HideScreen is null)
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

    private sealed class NativePcmAudioApi(HostAudioApiTable* table) : IHostPcmAudioApi
    {
        public bool PlayActionSound(ulong id,float gain,float x,float y,float z)=>table->PlayActionSound(id,gain,x,y,z)==0;
        public bool RegisterPcm16(System.ReadOnlySpan<byte> samples,uint rate,uint channels,out ulong clip)
        {
            clip=0;if(samples.Length==0 || samples.Length>16777216)return false;
            ulong value=0;fixed(byte* bytes=samples)if(table->RegisterPcm16(bytes,(uint)samples.Length,rate,channels,&value)!=0)return false;
            clip=value;return value!=0;
        }
        public bool PlayClip(ulong clip,float gain,bool loop,bool nonspatial,float x,float y,float z,out ulong voice)
        {
            ulong value=0;var result=table->PlayClip(clip,gain,(loop?1u:0)|(nonspatial?2u:0),x,y,z,&value);
            voice=value;return result==0 && value!=0;
        }
        public bool StopVoice(ulong voice)=>table->StopVoice(voice)==0;
        public bool ReleaseClip(ulong clip)=>table->ReleaseClip(clip)==0;
        public bool TryGetVoicePlaying(ulong voice,out bool playing){uint state=0;var result=table->QueryVoice(voice,&state);playing=state!=0;return result==0;}
    }

    private sealed class NativeUiApi : IHostUiApi, IDeclaredScreenBackend
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

        public bool PresentDeclaredScreen(string declaration, string fields)
        {
            var d = Encoding.UTF8.GetBytes(declaration + '\0');
            var f = Encoding.UTF8.GetBytes(fields + '\0');
            fixed (byte* dp = d, fp = f) return _table->PresentScreen(dp, fp) == 0;
        }

        public bool HideDeclaredScreen(string id)
        {
            var bytes = Encoding.UTF8.GetBytes(id + '\0');
            fixed (byte* p = bytes) return _table->HideScreen(p) == 0;
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
