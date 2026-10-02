using System.Runtime.InteropServices;
using System.Text;
namespace Octaryn.Shared.Host.Api;
[StructLayout(LayoutKind.Sequential,Pack=8,Size=192)]
internal unsafe struct HostRegionStatusNative
{
    public uint Version,Size,Index,Phase,Flags,Reserved;
    public ulong Generation;
    public fixed float Bounds[6];
    public fixed byte Id[129];
    public fixed byte Padding[7];
}
[StructLayout(LayoutKind.Sequential,Pack=4,Size=12)]
internal struct HostRegionAnchorNative {public float X,Y,Z;}
[StructLayout(LayoutKind.Sequential,Pack=8,Size=40)]
internal unsafe struct HostResidencyTable
{
    public uint Version,Size;
    public delegate* unmanaged[Cdecl]<uint*,ulong*,int> Count;
    public delegate* unmanaged[Cdecl]<uint,HostRegionStatusNative*,int> Query;
    public delegate* unmanaged[Cdecl]<uint*,uint,uint*,uint,int> SetDesired;
    public delegate* unmanaged[Cdecl]<HostRegionAnchorNative*,int> ActorPosition;
}
internal unsafe sealed partial class NativeHostApiProvider
{
    public IHostResidencyApi? GetResidencyApi()
    {
        if(_query is null)return null;
        var table=(HostResidencyTable*)_query(HostApiTableIds.Residency,1);
        return table is null || table->Version!=1 || table->Size<sizeof(HostResidencyTable) || table->Count is null || table->Query is null || table->SetDesired is null || table->ActorPosition is null
            ? null : new NativeResidencyApi(table);
    }
    private sealed class NativeResidencyApi(HostResidencyTable* table) : IHostResidencyApi
    {
        public bool TryGetActorPosition(out HostRegionAnchor position)
        {
            position=default;HostRegionAnchorNative native=default;
            if(table->ActorPosition(&native)!=0 || !float.IsFinite(native.X) || !float.IsFinite(native.Y) || !float.IsFinite(native.Z))return false;
            position=new(native.X,native.Y,native.Z);return true;
        }
        public bool SetDesiredRegions(ReadOnlySpan<uint> wanted,ReadOnlySpan<uint> retained)
        {
            if(wanted.Length>65536 || retained.Length>65536)return false;
            fixed(uint* requested=wanted,kept=retained)return table->SetDesired(requested,(uint)wanted.Length,kept,(uint)retained.Length)==0;
        }
        public bool TryGetCount(out uint count,out ulong generation)
        {
            uint total=0;ulong revision=0;var ok=table->Count(&total,&revision)==0 && total is >0 and <=65536;
            count=ok?total:0;generation=ok?revision:0;return ok;
        }
        public bool TryGetRegion(uint index,out HostRegionResidency region)
        {
            region=default;var native=new HostRegionStatusNative{Version=1,Size=192};
            if(table->Query(index,&native)!=0 || native.Version!=1 || native.Size!=192 || native.Index!=index ||
                native.Phase>5 || (native.Flags&~31u)!=0 || native.Reserved!=0)return false;
            for(var pad=0;pad<7;++pad)if(native.Padding[pad]!=0)return false;
            for(var axis=0;axis<3;++axis)if(!float.IsFinite(native.Bounds[axis]) || !float.IsFinite(native.Bounds[axis+3]) || native.Bounds[axis]>native.Bounds[axis+3])return false;
            var length=0;while(length<129 && native.Id[length]!=0){if(native.Id[length] is <33 or >126)return false;++length;}
            if(length is 0 or >128)return false;
            region=new(index,Encoding.ASCII.GetString(native.Id,length),(HostRegionPhase)native.Phase,(HostRegionFlags)native.Flags,native.Generation,
                new(native.Bounds[0],native.Bounds[1],native.Bounds[2],native.Bounds[3],native.Bounds[4],native.Bounds[5]));return true;
        }
    }
}
