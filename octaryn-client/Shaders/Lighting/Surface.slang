#ifndef OCTARYN_LIGHTING_SURFACE
#define OCTARYN_LIGHTING_SURFACE
uint lighting_voxel(float4 value) {
    uint4 p=uint4(round(value*255));return p.x|(p.y<<8)|(p.z<<16)|(p.w<<24);
}
float3 lighting_normal(uint voxel) {
    const float3 normals[6]={float3(0,0,1),float3(0,0,-1),float3(1,0,0),float3(-1,0,0),float3(0,1,0),float3(0,-1,0)};
    return normals[min(voxel&7,5u)];
}
#endif
