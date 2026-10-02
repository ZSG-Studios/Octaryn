using System.Runtime.InteropServices;
using System.Text.Json;
namespace Octaryn.Server.World.MapWorld;

internal sealed class SceneBodyDefinition
{
    public ulong SourceId { get; set; }
    public string Reference { get; set; }="";
    public bool PickupAllowed { get; set; }
    public string ItemBase { get; set; }="";
    public uint Count { get; set; }=1;
    public float[] Position { get; set; }=[];
    public float[] Rotation { get; set; }=[0,0,0,1];
    public float Mass { get; set; }
    public float LinearDamping { get; set; }
    public float AngularDamping { get; set; }
    public float Friction { get; set; }
    public float Restitution { get; set; }
    public float[] Inertia { get; set; }=new float[9];
    public float[] Center { get; set; }=new float[3];
    public SceneShapeDefinition[] Shapes { get; set; }=[];
}
internal sealed class SceneShapeDefinition
{
    public uint Kind { get; set; }
    public float[] Points { get; set; }=[];
    public float[] LocalPosition { get; set; }=new float[3];
    public float[] LocalRotation { get; set; }=[0,0,0,1];
    public float[] HalfExtents { get; set; }=new float[3];
    public float[] CapsuleA { get; set; }=new float[3];
    public float[] CapsuleB { get; set; }=new float[3];
    public float Radius { get; set; }
}
internal sealed class SceneBodyCatalogFile
{
    public int Version { get; set; }
    public SceneBodyDefinition[] Bodies { get; set; }=[];
}
internal sealed class SceneBodyEntry(SceneBodyDefinition source)
{
    internal ulong Handle;
    internal readonly SceneBodyDefinition Source=source;
}
internal static unsafe class SceneBodyCatalog
{
    internal static Dictionary<ulong,SceneBodyEntry> Load(IntPtr world,string glb)
    {
        var result=new Dictionary<ulong,SceneBodyEntry>();
        var path=Path.ChangeExtension(glb,"physics.json");
        if(!File.Exists(path))return result;
        if(new FileInfo(path).Length>16777216)throw new InvalidDataException("Scene physics catalogue exceeds its bound.");
        var source=JsonSerializer.Deserialize<SceneBodyCatalogFile>(File.ReadAllBytes(path),new JsonSerializerOptions{PropertyNameCaseInsensitive=true});
        if(source?.Version!=1 || source.Bodies.Length>8192)throw new InvalidDataException("Invalid scene physics catalogue.");
        foreach(var body in source.Bodies)
        {
            if(body.SourceId==0 || body.Shapes.Length is 0 or >256 || result.ContainsKey(body.SourceId) ||
                body.PickupAllowed && (body.Count is 0 or >1000000 || string.IsNullOrWhiteSpace(body.ItemBase)))
                throw new InvalidDataException("Invalid scene body identity or collectible metadata.");
            Validate(body); // Reject what native creation would reject before activation is scheduled.
            result.Add(body.SourceId,new(body));
        }
        return result;
    }
    internal static bool TryCreate(IntPtr world,SceneBodyDefinition body,out ulong handle)
    {
        handle=0;
            var pins=new List<GCHandle>();
            try
            {
                var shapes=new SceneBodyShapeNative[body.Shapes.Length];
                for(var i=0;i<shapes.Length;++i)
                {
                    var src=body.Shapes[i];var shape=default(SceneBodyShapeNative);
                    shape.Kind=src.Kind;shape.Radius=src.Radius;
                    Copy(src.LocalPosition,shape.LocalPosition,3);Copy(src.LocalRotation,shape.LocalRotation,4);
                    Copy(src.HalfExtents,shape.HalfExtents,3);Copy(src.CapsuleA,shape.CapsuleA,3);Copy(src.CapsuleB,shape.CapsuleB,3);
                    if(src.Points.Length>768 || src.Points.Length%3!=0 || src.Points.Any(v=>!float.IsFinite(v)))
                        throw new InvalidDataException("Invalid scene convex hull.");
                    if(src.Points.Length>0)
                    {
                        var pin=GCHandle.Alloc(src.Points,GCHandleType.Pinned);pins.Add(pin);
                        shape.Points=(float*)pin.AddrOfPinnedObject();shape.PointCount=(uint)(src.Points.Length/3);
                    }
                    shapes[i]=shape;
                }
                fixed(SceneBodyShapeNative* pointer=shapes)
                {
                    var desc=new SceneBodyDescNative{SourceId=body.SourceId,ShapeCount=(uint)shapes.Length,Shapes=pointer,
                        Mass=body.Mass,LinearDamping=body.LinearDamping,AngularDamping=body.AngularDamping,Friction=body.Friction,Restitution=body.Restitution};
                    Copy(body.Position,desc.Position,3);Copy(body.Rotation,desc.Rotation,4);
                    Copy(body.Inertia,desc.Inertia,9);Copy(body.Center,desc.Center,3);
                    if(world==IntPtr.Zero)return false;
                    ulong created=0;
                    var status=NativeMapWorld.CreateBody(world,&desc,&created);
                    if(status==2)return false;
                    if(status!=0 || created==0)
                        throw new InvalidDataException($"Source scene body creation failed for {body.Reference}.");
                    handle=created;return true;
                }
            }
            finally {foreach(var pin in pins)pin.Free();}
    }
    // Mirrors SceneBodies::create and attach_scene_body_shape so a bad entry fails at load, not mid-session.
    private static void Validate(SceneBodyDefinition body)
    {
        TryCreate(IntPtr.Zero,body,out _);
        if(!Rotation(body.Rotation) || !(body.Mass>0) || body.LinearDamping<0 || body.AngularDamping<0 ||
            body.Friction<0 || body.Restitution is <0 or >1)
            throw new InvalidDataException($"Invalid scene body dynamics for {body.Reference}.");
        foreach(var shape in body.Shapes)
        {
            var valid=Rotation(shape.LocalRotation) && shape.Kind switch
            {
                1=>shape.HalfExtents.All(v=>v>0),
                2 or 3=>shape.Radius>0,
                4=>shape.Points.Length/3 is >=4 and <=1024,
                _=>false
            };
            if(!valid)throw new InvalidDataException($"Invalid scene body shape for {body.Reference}.");
        }
    }
    private static bool Rotation(float[] q)=>q.Length==4 && q.All(float.IsFinite) &&
        MathF.Abs(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]-1)<0.01f;
    private static void Copy(float[] source,float* destination,int count)
    {
        if(source.Length!=count || source.Any(v=>!float.IsFinite(v)))throw new InvalidDataException("Invalid scene physics vector.");
        for(var i=0;i<count;++i)destination[i]=source[i];
    }
}
