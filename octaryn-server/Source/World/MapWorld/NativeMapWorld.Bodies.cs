using System.Runtime.InteropServices;
using Octaryn.Shared.Host.Api;
namespace Octaryn.Server.World.MapWorld;

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct SceneBodyShapeNative
{
    public uint Kind, PointCount;
    public float* Points;
    public fixed float LocalPosition[3], LocalRotation[4], HalfExtents[3], CapsuleA[3], CapsuleB[3];
    public float Radius;
}
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct SceneBodyDescNative
{
    public ulong SourceId;
    public uint ShapeCount, Flags;
    public SceneBodyShapeNative* Shapes;
    public fixed float Position[3], Rotation[4];
    public float Mass, LinearDamping, AngularDamping, Friction, Restitution;
    public fixed float Inertia[9], Center[3];
}
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct SceneBodyPoseNative
{
    public ulong Handle, SourceId;
    public fixed float Position[3], Rotation[4], Velocity[3], AngularVelocity[3];
    public uint Flags;
    public readonly HostSceneBodyPose Managed()
    {
        fixed(float* p=Position,q=Rotation,v=Velocity)
            return new(SourceId,p[0],p[1],p[2],q[0],q[1],q[2],q[3],v[0],v[1],v[2],Flags);
    }
}
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct SceneBodyHitNative
{
    public ulong Handle, SourceId;
    public fixed float Point[3], Normal[3];
    public float Distance;
    public readonly HostSceneBodyHit Managed()
    {
        fixed(float* p=Point,n=Normal) return new(SourceId,p[0],p[1],p[2],n[0],n[1],n[2],Distance);
    }
}
internal static unsafe partial class NativeMapWorld
{
    private static class Bodies
    {
        static Bodies()
        {
            if(sizeof(SceneBodyShapeNative)!=88 || sizeof(SceneBodyDescNative)!=120 ||
                sizeof(SceneBodyPoseNative)!=72 || sizeof(SceneBodyHitNative)!=48)
                throw new TypeLoadException("Scene body native/managed ABI layout changed.");
        }
        internal static readonly delegate* unmanaged[Cdecl]<IntPtr,SceneBodyDescNative*,ulong*,int> Create =
            (delegate* unmanaged[Cdecl]<IntPtr,SceneBodyDescNative*,ulong*,int>)Export("create");
        internal static readonly delegate* unmanaged[Cdecl]<IntPtr,ulong,int> Remove =
            (delegate* unmanaged[Cdecl]<IntPtr,ulong,int>)Export("remove");
        internal static readonly delegate* unmanaged[Cdecl]<IntPtr,ulong,SceneBodyPoseNative*,int> Pose =
            (delegate* unmanaged[Cdecl]<IntPtr,ulong,SceneBodyPoseNative*,int>)Export("pose");
        internal static readonly delegate* unmanaged[Cdecl]<IntPtr,float*,float*,float,SceneBodyHitNative*,int> Ray =
            (delegate* unmanaged[Cdecl]<IntPtr,float*,float*,float,SceneBodyHitNative*,int>)Export("ray");
        internal static readonly delegate* unmanaged[Cdecl]<IntPtr,ulong,float*,float*,float,int> Grab =
            (delegate* unmanaged[Cdecl]<IntPtr,ulong,float*,float*,float,int>)Export("grab");
        internal static readonly delegate* unmanaged[Cdecl]<IntPtr,float*,int> Move =
            (delegate* unmanaged[Cdecl]<IntPtr,float*,int>)Export("grab_move");
        internal static readonly delegate* unmanaged[Cdecl]<IntPtr,int> Release =
            (delegate* unmanaged[Cdecl]<IntPtr,int>)Export("grab_release");
        internal static readonly delegate* unmanaged[Cdecl]<IntPtr,double,int> Step =
            (delegate* unmanaged[Cdecl]<IntPtr,double,int>)Export("step");
        private static IntPtr Export(string suffix) => NativeLibrary.GetExport(s_library,"octaryn_server_map_world_body_"+suffix);
    }
    internal static int CreateBody(IntPtr world,SceneBodyDescNative* desc,ulong* handle) => Bodies.Create(world,desc,handle);
    internal static int RemoveBody(IntPtr world,ulong handle) => Bodies.Remove(world,handle);
    internal static bool BodyPose(IntPtr world,ulong handle,out SceneBodyPoseNative pose)
    {
        pose=default;fixed(SceneBodyPoseNative* p=&pose)return Bodies.Pose(world,handle,p)==0;
    }
    internal static bool BodyRay(IntPtr world,float* origin,float* direction,float reach,out SceneBodyHitNative hit)
    {
        hit=default;fixed(SceneBodyHitNative* p=&hit)return Bodies.Ray(world,origin,direction,reach,p)==0;
    }
    internal static bool GrabBody(IntPtr world,ulong handle,float* hit,float* target,float force) => Bodies.Grab(world,handle,hit,target,force)==0;
    internal static bool MoveBodyGrab(IntPtr world,float* target) => Bodies.Move(world,target)==0;
    internal static void ReleaseBodyGrab(IntPtr world) => Bodies.Release(world);
    internal static int StepBodies(IntPtr world,double dt) => Bodies.Step(world,dt);
}
