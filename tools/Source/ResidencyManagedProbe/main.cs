using System.Runtime.InteropServices;
using Octaryn.Shared.Host.Api;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.ApiExposure;
namespace Octaryn.Shared.Host.Api {
 internal unsafe sealed partial class NativeHostApiProvider {
  private readonly delegate* unmanaged[Cdecl]<uint,uint,void*> _query;
  public NativeHostApiProvider(delegate* unmanaged[Cdecl]<uint,uint,void*> query){_query=query;}
 }
}
unsafe class Program {
 static HostResidencyTable table;static uint mode;static int checks;
 static void Check(bool ok){++checks;if(!ok)throw new Exception("FAIL "+checks);}
 [UnmanagedCallersOnly(CallConvs=[typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
 static void* Query(uint id,uint version){return id==15 && version==1?System.Runtime.CompilerServices.Unsafe.AsPointer(ref table):null;}
 [UnmanagedCallersOnly(CallConvs=[typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
 static int Count(uint* count,ulong* revision){*count=mode==1?65537u:2u;*revision=7;return 0;}
 [UnmanagedCallersOnly(CallConvs=[typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
 static int Region(uint index,HostRegionStatusNative* value){
  *value=new(){Version=1,Size=192,Index=index,Phase=5,Flags=15,Generation=7};
  value->Bounds[3]=value->Bounds[4]=value->Bounds[5]=10;
  var text="cell-a/0";for(var k=0;k<text.Length;++k)value->Id[k]=(byte)text[k];
  if(mode==2)value->Phase=6;if(mode==3)value->Flags=32;if(mode==4)value->Bounds[0]=float.NaN;
  if(mode==5)value->Bounds[0]=11;if(mode==6)value->Id[1]=32;if(mode==7)value->Index=index+1;if(mode==9)value->Padding[0]=1;
  return mode==8?1:0;
 }
 [UnmanagedCallersOnly(CallConvs=[typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
 static int SetDesired(uint* wanted,uint count,uint* retained,uint keep){return count==1&&keep==2&&wanted[0]==1&&retained[0]==0&&retained[1]==1?0:1;}
 [UnmanagedCallersOnly(CallConvs=[typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
 static int Actor(HostRegionAnchorNative* value){*value=new(){X=mode==11?float.NaN:1,Y=2,Z=3};return mode==10?1:0;}
 static void Main(){
  Check(sizeof(HostRegionStatusNative)==192);Check(sizeof(HostResidencyTable)==40);
  table=new(){Version=1,Size=40,Count=&Count,Query=&Region,SetDesired=&SetDesired,ActorPosition=&Actor};var api=new NativeHostApiProvider(&Query).GetResidencyApi();Check(api!=null);
  Check(api!.TryGetCount(out var count,out var generation)&&count==2&&generation==7);
  Check(api.TryGetRegion(0,out var region)&&region.Id=="cell-a/0"&&region.Flags==(HostRegionFlags)15&&region.Bounds.MaxX==10);
  Check(api.TryGetActorPosition(out var actor)&&actor==new HostRegionAnchor(1,2,3));
  mode=10;Check(!api.TryGetActorPosition(out actor));mode=11;Check(!api.TryGetActorPosition(out actor));mode=0;
  Check(api.SetDesiredRegions([1],[0,1]));Check(!api.SetDesiredRegions([0],[0]));
  var read=new GrantedResidencyApi(api,new(){RequestedHostApis=[HostApiIds.Residency],Schedule=new(){Systems=[new(){Reads=[new(){ResourceId=HostApiIds.Residency,Mode=Octaryn.Shared.Host.ScheduledAccessMode.Read}]}]}});
  Check(read.TryGetActorPosition(out _));Check(read.TryGetCount(out _,out _));Check(!read.SetDesiredRegions([1],[0,1]));
  var write=new GrantedResidencyApi(api,new(){RequestedHostApis=[HostApiIds.Residency],Schedule=new(){Systems=[new(){Writes=[new(){ResourceId=HostApiIds.Residency,Mode=Octaryn.Shared.Host.ScheduledAccessMode.Write}]}]}});
  Check(write.TryGetRegion(0,out _));Check(write.SetDesiredRegions([1],[0,1]));write.Dispose();Check(!write.TryGetActorPosition(out _));Check(!write.TryGetCount(out _,out _));Check(!write.SetDesiredRegions([1],[0,1]));
  var denied=new GrantedResidencyApi(api,new());Check(!denied.TryGetActorPosition(out _));Check(!denied.TryGetCount(out _,out _));Check(!denied.SetDesiredRegions([1],[0,1]));
  mode=1;Check(!api.TryGetCount(out count,out generation)&&count==0);
  for(mode=2;mode<=9;++mode)Check(!api.TryGetRegion(0,out region));
  table.Version=2;Check(new NativeHostApiProvider(&Query).GetResidencyApi()==null);
  Console.WriteLine("PASS "+checks+" actual managed ABI assertions");
 }
}
