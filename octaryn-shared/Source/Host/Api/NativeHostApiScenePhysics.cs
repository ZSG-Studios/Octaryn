using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

namespace Octaryn.Shared.Host.Api;

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct HostScenePhysicsTable
{
    public uint Version, Size;
    public delegate* unmanaged[Cdecl]<byte*, int> Submit;
    public delegate* unmanaged[Cdecl]<byte*, uint, int> Snapshot;
}

internal unsafe sealed partial class NativeHostApiProvider
{
    public IHostScenePhysicsApi? GetScenePhysicsApi()
    {
        if (_query is null) return null;
        var table = (HostScenePhysicsTable*)_query(HostApiTableIds.ScenePhysics, 1);
        return table is null || table->Version != 1 || table->Size < sizeof(HostScenePhysicsTable)
            || table->Submit is null || table->Snapshot is null ? null : new NativeScenePhysicsApi(table);
    }

    private sealed class NativeScenePhysicsApi(HostScenePhysicsTable* table) : IHostScenePhysicsApi
    {
        private static long s_request;
        private readonly byte[] _buffer = new byte[4194304];
        private ScenePhysicsSnapshot? Read()
        {
            fixed (byte* bytes = _buffer)
            {
                if (table->Snapshot(bytes, (uint)_buffer.Length) != 0) return null;
            }
            var length = Array.IndexOf(_buffer, (byte)0);
            if (length < 1) return null;
            try
            {
                var snapshot = JsonSerializer.Deserialize<ScenePhysicsSnapshot>(_buffer.AsSpan(0, length));
                return snapshot?.Version == 1 && snapshot.Bodies is { Length: <=8192 } &&
                    snapshot.Receipts is { Length: <=256 } ? snapshot : null;
            }
            catch (JsonException) { return null; }
        }
        private ulong Send(ScenePhysicsCommand command)
        {
            command.RequestId = (ulong)System.Threading.Interlocked.Increment(ref s_request);
            var bytes = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(command) + '\0');
            fixed (byte* pointer = bytes) return table->Submit(pointer) == 0 ? command.RequestId : 0;
        }
        private static bool Finite(params float[] values) => values.All(float.IsFinite);
        public bool SetAim(float x,float y,float z,float dx,float dy,float dz,float reach) =>
            Finite(x,y,z,dx,dy,dz,reach) && reach > 0 && reach <= 5 &&
            Send(new(){Kind="aim",X=x,Y=y,Z=z,Dx=dx,Dy=dy,Dz=dz,Reach=reach}) != 0;
        public bool TryGetTarget(out HostSceneBodyHit hit)
        {
            hit = default;
            if (Read()?.Target is not { } value || value.SourceId == 0) return false;
            hit = value; return true;
        }
        public ulong Grab(ulong sourceId,float hitX,float hitY,float hitZ,float targetX,float targetY,float targetZ) =>
            sourceId != 0 && Finite(hitX,hitY,hitZ,targetX,targetY,targetZ) ?
            Send(new(){Kind="grab",SourceId=sourceId,HitX=hitX,HitY=hitY,HitZ=hitZ,X=targetX,Y=targetY,Z=targetZ}) : 0;
        public bool MoveGrab(float x,float y,float z) => Finite(x,y,z) && Send(new(){Kind="move",X=x,Y=y,Z=z}) != 0;
        public ulong Release() => Send(new(){Kind="release"});
        public ulong Pickup(ulong sourceId) => sourceId != 0 ? Send(new(){Kind="pickup",SourceId=sourceId}) : 0;
        public bool TryGetReceipt(ulong requestId,out HostScenePhysicsReceipt receipt)
        {
            receipt = default;
            foreach(var value in Read()?.Receipts ?? []) if(value.RequestId == requestId) {receipt=value;return true;}
            return false;
        }
        public bool TryGetPose(ulong sourceId,out HostSceneBodyPose pose)
        {
            pose=default;
            foreach(var value in Read()?.Bodies ?? []) if(value.SourceId==sourceId) {pose=value;return true;}
            return false;
        }
        public bool TryGetInventory(out IReadOnlyList<HostSceneInventoryCount> inventory)
        {
            inventory=[];
            if(Read() is not { Inventory: { Length: <=4096 } } snapshot)return false;
            inventory=snapshot.Inventory;return true;
        }
    }
}
