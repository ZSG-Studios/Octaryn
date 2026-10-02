using System.Runtime.InteropServices;
using System.Text;

namespace Octaryn.Shared.Host.Api;

[StructLayout(LayoutKind.Sequential)]
internal struct HostTransitionPoseNative {public double X,Y,Z;public float Yaw,Pitch;}
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct HostTransitionViewNative {public HostTransitionPoseNative Pose;public uint Enabled,Reserved;public fixed byte AssetId[256];}
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct HostTransitionApiTable
{
    public uint Version,Size;
    public delegate* unmanaged[Cdecl]<HostTransitionViewNative*,int> ReadView;
    public delegate* unmanaged[Cdecl]<byte*,byte*,HostTransitionPoseNative*,ulong*,int> Begin;
    public delegate* unmanaged[Cdecl]<ulong,uint*,byte*,uint,int> Status;
    public delegate* unmanaged[Cdecl]<ulong,int> Cancel;
}
internal unsafe sealed partial class NativeHostApiProvider
{
    public IHostSceneTransitionApi? GetTransitionApi(GameModules.GameModuleManifest manifest,string moduleRoot)
    {
        if(_query is null)return null;
        var table=(HostTransitionApiTable*)_query(HostApiTableIds.Transition,1);
        if(table is null || table->Version<1 || table->Size<sizeof(HostTransitionApiTable) ||
            table->ReadView is null || table->Begin is null || table->Status is null || table->Cancel is null)return null;
        return new DeclaredSceneTransitionApi(new NativeTransition(table),manifest,moduleRoot);
    }
    private sealed class NativeTransition(HostTransitionApiTable* table) : ISceneTransitionBackend
    {
        public bool TryGetView(out HostTransitionView view)
        {
            view=default;HostTransitionViewNative wire=default;
            if(table->ReadView(&wire)!=0 || wire.Enabled>1)return false;
            var id=Read(wire.AssetId,256);if(id.Length==0)return false;
            var p=wire.Pose;view=new(id,new(p.X,p.Y,p.Z,p.Yaw,p.Pitch),wire.Enabled!=0);return view.Pose.IsValid;
        }
        public bool Begin(string id,string path,in HostTransitionPose pose,out ulong revision,out string error)
        {
            revision=0;error="Client refused scene transition (disabled, busy, or invalid request).";
            var a=Encoding.UTF8.GetBytes(id+"\0");var b=Encoding.UTF8.GetBytes(path+"\0");
            if(a.Length>256 || b.Length>4096)return false;
            var p=new HostTransitionPoseNative{X=pose.X,Y=pose.Y,Z=pose.Z,Yaw=pose.Yaw,Pitch=pose.Pitch};ulong r=0;
            fixed(byte* ap=a,bp=b)if(table->Begin(ap,bp,&p,&r)!=0 || r==0)return false;
            revision=r;error="";return true;
        }
        public bool TryGetStatus(ulong revision,out HostTransitionState state,out string error)
        {
            state=default;error="Transition revision is unavailable.";uint raw=0;byte* message=stackalloc byte[1024];message[0]=0;
            if(table->Status(revision,&raw,message,1024)!=0 || raw>3)return false;
            state=(HostTransitionState)raw;error=Read(message,1024);return true;
        }
        public bool Cancel(ulong revision)=>table->Cancel(revision)==0;
        private static string Read(byte* text,int capacity)
        {var size=0;while(size<capacity && text[size]!=0)++size;return size==capacity?"":Encoding.UTF8.GetString(new ReadOnlySpan<byte>(text,size));}
    }
}
