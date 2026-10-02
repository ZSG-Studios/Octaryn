namespace Octaryn.Shared.Navigation;

// Coordinates retain the caller's units and axes; this library applies no conversion.
public readonly record struct NavigationPoint(double X, double Y, double Z)
{
    internal bool Valid => double.IsFinite(X) && double.IsFinite(Y) && double.IsFinite(Z) &&
        System.Math.Abs(X) <= 20_000_000 && System.Math.Abs(Y) <= 20_000_000 && System.Math.Abs(Z) <= 20_000_000;
    internal static double Distance(NavigationPoint a, NavigationPoint b) =>
        System.Math.Sqrt((a.X-b.X)*(a.X-b.X)+(a.Y-b.Y)*(a.Y-b.Y)+(a.Z-b.Z)*(a.Z-b.Z));
    internal static NavigationPoint Midpoint(NavigationPoint a, NavigationPoint b) =>
        new((a.X+b.X)/2, (a.Y+b.Y)/2, (a.Z+b.Z)/2);
}

// Neighbor slots correspond to edges 0-1, 1-2 and 2-0. -1 is a closed boundary.
public readonly record struct NavigationTriangle(int Vertex0, int Vertex1, int Vertex2,
    int Neighbor0, int Neighbor1, int Neighbor2, bool Enabled = true)
{
    internal int Vertex(int edge) => edge == 0 ? Vertex0 : edge == 1 ? Vertex1 : Vertex2;
    internal int Neighbor(int edge) => edge == 0 ? Neighbor0 : edge == 1 ? Neighbor1 : Neighbor2;
}

public readonly record struct NavigationPortal(int From, int To, NavigationPoint Endpoint0, NavigationPoint Endpoint1);

public sealed class NavigationCorridor
{
    public System.Collections.Generic.IReadOnlyList<int> Triangles { get; }
    public System.Collections.Generic.IReadOnlyList<NavigationPortal> Portals { get; }
    public System.Collections.Generic.IReadOnlyList<NavigationPoint> Points { get; }
    public double GraphCost { get; }
    public int Expansions { get; }
    internal NavigationCorridor(int[] triangles, NavigationPortal[] portals, NavigationPoint[] points,
        double cost, int expansions)
    {
        Triangles = System.Array.AsReadOnly(triangles); Portals = System.Array.AsReadOnly(portals);
        Points = System.Array.AsReadOnly(points); GraphCost = cost; Expansions = expansions;
    }
}
