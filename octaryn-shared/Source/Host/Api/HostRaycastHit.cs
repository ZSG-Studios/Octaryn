namespace Octaryn.Shared.Host.Api;

public readonly record struct HostRaycastHit(
    uint MaterialId,
    float PointX,
    float PointY,
    float PointZ,
    float NormalX,
    float NormalY,
    float NormalZ,
    float Distance,
    uint TriangleIndex);
