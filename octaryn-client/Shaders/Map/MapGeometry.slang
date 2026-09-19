#ifndef OCTARYN_MAP_GEOMETRY_SLANG
#define OCTARYN_MAP_GEOMETRY_SLANG
// Shared GLB map layout for the raster and ray-query paths.
// 32-byte vertex: float3 position, float3 normal, float2 uv.
static const uint kMapRayInstanceId=0x4D4150u; // 'MAP' TLAS instance user id.
struct MapVertex {
    float position[3];
    float normal[3];
    float2 uv;
};
struct MapRayMaterial {
    float baseColor[4];
    float alphaCutoff;
    uint alphaMode; // 0 opaque, 1 mask, 2 blend.
};
float3 map_vertex_position(MapVertex vertex) {
    return float3(vertex.position[0],vertex.position[1],vertex.position[2]);
}
float3 map_vertex_normal(MapVertex vertex) {
    return float3(vertex.normal[0],vertex.normal[1],vertex.normal[2]);
}
float4 map_ray_base_color(MapRayMaterial material) {
    return float4(material.baseColor[0],material.baseColor[1],
        material.baseColor[2],material.baseColor[3]);
}
#endif
