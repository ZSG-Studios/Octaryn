using System.Collections.Generic;

namespace Octaryn.Shared.Navigation;

public sealed partial class NavigationMesh
{
    // Searches only validated internal adjacency. This does not move an actor.
    public bool TryFindCorridor(int startTriangle,int goalTriangle,out NavigationCorridor? path,
        out string error,int maxExpansions=4096)
    {
        path=null;error="Navigation query triangle or expansion budget is invalid.";
        if((uint)startTriangle>=(uint)TriangleCount || (uint)goalTriangle>=(uint)TriangleCount ||
           maxExpansions<1 || maxExpansions>MaximumTriangles)return false;
        if(!_triangles[startTriangle].Enabled || !_triangles[goalTriangle].Enabled)
        {error="Navigation query starts or ends on a blocked triangle.";return false;}
        var costs=new double[TriangleCount];System.Array.Fill(costs,double.PositiveInfinity);
        var previous=new int[TriangleCount];System.Array.Fill(previous,-1);var closed=new bool[TriangleCount];
        var queue=new PriorityQueue<int,(double Cost,int Triangle)>();
        costs[startTriangle]=0;queue.Enqueue(startTriangle,(NavigationPoint.Distance(_centres[startTriangle],_centres[goalTriangle]),startTriangle));
        var expansions=0;
        while(queue.TryDequeue(out var current,out _))
        {
            if(closed[current])continue;
            if(expansions>=maxExpansions) {error="Navigation query expansion budget exhausted.";return false;}
            closed[current]=true;++expansions;
            if(current==goalTriangle)
            {path=Reconstruct(startTriangle,goalTriangle,previous,costs[current],expansions);error=string.Empty;return true;}
            for(var edge=0;edge<3;++edge)
            {
                var next=_triangles[current].Neighbor(edge);
                if(next<0 || closed[next] || !_triangles[next].Enabled)continue;
                var candidate=costs[current]+NavigationPoint.Distance(_centres[current],_centres[next]);
                if(candidate>=costs[next])continue;
                costs[next]=candidate;previous[next]=current;
                // Each expanded triangle adds at most three entries, including stale entries.
                if(queue.Count>=MaximumTriangles*3) {error="Navigation query queue admission exhausted.";return false;}
                queue.Enqueue(next,(candidate+NavigationPoint.Distance(_centres[next],_centres[goalTriangle]),next));
            }
        }
        error="No internal navigation corridor connects these triangles.";return false;
    }

    private NavigationCorridor Reconstruct(int start,int goal,int[] previous,double cost,int expansions)
    {
        var reverse=new List<int>();
        for(var triangle=goal;triangle>=0;triangle=previous[triangle])
        {reverse.Add(triangle);if(triangle==start)break;}
        reverse.Reverse();var portals=new NavigationPortal[reverse.Count-1];
        var points=new NavigationPoint[reverse.Count*2-1];
        for(var index=0;index<reverse.Count;++index)
        {
            var triangle=reverse[index];points[index*2]=_centres[triangle];if(index+1==reverse.Count)continue;
            var next=reverse[index+1];var source=_triangles[triangle];
            for(var edge=0;edge<3;++edge)if(source.Neighbor(edge)==next)
            {
                var a=_vertices[source.Vertex(edge)];var b=_vertices[source.Vertex((edge+1)%3)];
                portals[index]=new(triangle,next,a,b);points[index*2+1]=NavigationPoint.Midpoint(a,b);break;
            }
        }
        return new(reverse.ToArray(),portals,points,cost,expansions);
    }
}
