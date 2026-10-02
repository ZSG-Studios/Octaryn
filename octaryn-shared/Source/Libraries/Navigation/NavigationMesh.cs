namespace Octaryn.Shared.Navigation;

public sealed partial class NavigationMesh
{
    public const int MaximumVertices = 65536, MaximumTriangles = 32768;
    private readonly NavigationPoint[] _vertices, _centres;
    private readonly NavigationTriangle[] _triangles;
    public int VertexCount => _vertices.Length;
    public int TriangleCount => _triangles.Length;
    public int EnabledTriangles { get; }
    public NavigationPoint GetVertex(int index) => _vertices[index];
    public NavigationTriangle GetTriangle(int index) => _triangles[index];
    public NavigationPoint GetCentre(int index) => _centres[index];

    private NavigationMesh(NavigationPoint[] vertices, NavigationTriangle[] triangles, NavigationPoint[] centres, int enabled)
    { _vertices=vertices; _triangles=triangles; _centres=centres; EnabledTriangles=enabled; }

    public static bool TryCreate(System.ReadOnlySpan<NavigationPoint> vertices,
        System.ReadOnlySpan<NavigationTriangle> triangles, out NavigationMesh? mesh, out string error)
    {
        mesh=null; error="Navigation mesh exceeds the vertex or triangle admission limit.";
        if(vertices.Length is <3 or >MaximumVertices || triangles.Length is <1 or >MaximumTriangles)return false;
        var ownedVertices=vertices.ToArray(); var ownedTriangles=triangles.ToArray();
        foreach(var vertex in ownedVertices)if(!vertex.Valid) {error="Navigation coordinate is nonfinite or unbounded.";return false;}
        var centres=new NavigationPoint[triangles.Length]; var enabled=0;
        for(var index=0;index<ownedTriangles.Length;++index)
        {
            var triangle=ownedTriangles[index];
            for(var edge=0;edge<3;++edge)
                if((uint)triangle.Vertex(edge)>=(uint)ownedVertices.Length || triangle.Neighbor(edge)<-1 ||
                   triangle.Neighbor(edge)>=ownedTriangles.Length || triangle.Neighbor(edge)==index)
                {error="Navigation vertex or neighbor index is invalid.";return false;}
            var a=ownedVertices[triangle.Vertex0]; var b=ownedVertices[triangle.Vertex1]; var c=ownedVertices[triangle.Vertex2];
            centres[index]=new((a.X+b.X+c.X)/3,(a.Y+b.Y+c.Y)/3,(a.Z+b.Z+c.Z)/3);
            if(!triangle.Enabled)continue;
            var ux=b.X-a.X;var uy=b.Y-a.Y;var uz=b.Z-a.Z;
            var vx=c.X-a.X;var vy=c.Y-a.Y;var vz=c.Z-a.Z;
            var nx=uy*vz-uz*vy;var ny=uz*vx-ux*vz;var nz=ux*vy-uy*vx;
            if(nx*nx+ny*ny+nz*nz<=1e-18)
            {error="Enabled navigation triangle is degenerate.";return false;}
            ++enabled;
        }
        // A declared neighbor must meet on the exact same edge and point back.
        for(var index=0;index<ownedTriangles.Length;++index)for(var edge=0;edge<3;++edge)
        {
            var triangle=ownedTriangles[index];var neighbor=triangle.Neighbor(edge);if(neighbor<0)continue;
            var target=ownedTriangles[neighbor];var a=triangle.Vertex(edge);var b=triangle.Vertex((edge+1)%3);var matches=0;
            for(var other=0;other<3;++other)
            {
                var x=target.Vertex(other);var y=target.Vertex((other+1)%3);
                if((x==a && y==b || x==b && y==a) && target.Neighbor(other)==index)++matches;
            }
            if(matches!=1) {error="Navigation adjacency is not reciprocal on a shared edge.";return false;}
        }
        mesh=new(ownedVertices,ownedTriangles,centres,enabled);error=string.Empty;return true;
    }
}
