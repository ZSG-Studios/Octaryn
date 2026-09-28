#ifndef OCTARYN_WORLD_PLANT_SURFACE
#define OCTARYN_WORLD_PLANT_SURFACE
// Cube tags remain 0..5. Plant tags carry kind 6..9 and a five-bit atlas layer.
bool world_plant_tag(uint tag) {
    uint kind=tag&15,layer=tag>>4;
    return kind>=6 && kind<=9 && layer<29 && (layer<17 || layer>23);
}
bool world_transport_tag(uint tag) {return tag<6 || world_plant_tag(tag);}
uint world_plant_plane(uint tag) {return ((tag&15)-6)/2;}
float3 world_plant_normal(uint tag) {
    float3 normal=world_plant_plane(tag)==0?float3(-1,0,1):float3(1,0,1);
    return normal*(((tag&1)!=0)?-.7071067811865475:.7071067811865475);
}
bool world_crossed_plant(uint flags,uint voxel,uint direction) {
    uint layer=(voxel>>3)&31;
    return (flags&(16u|2u))==(16u|2u) && (flags&(4u|32u|(1u<<30)))==0 &&
        direction>=6 && direction<10 && layer<29 && (layer<17 || layer>23);
}
uint4 world_plant_surface_key(uint4 face,uint voxel,float3 toward) {
    uint direction=(face.w>>16)&15;
    if(direction<6 || direction>=10 || !all(isfinite(toward)))return uint4(0);
    uint kind=6+((direction-6)/2)*2,layer=(voxel>>3)&31;
    uint tag=kind|(layer<<4);
    if(!world_plant_tag(tag))return uint4(0);
    if(dot(world_plant_normal(tag),toward)<0)++tag;
    return uint4(face.xyz,tag+1);
}
float3 world_plant_offset(uint tag,float2 uv) {
    return float3(1-uv.x,1-uv.y,world_plant_plane(tag)==0?1-uv.x:uv.x);
}
// Bits 9..11 are unused by ordinary voxel material/direction encoding.
bool world_plant_voxel(uint voxel) {return (voxel&7u)!=6u && (voxel&768u)==768u;}
uint world_plant_pack_normal(uint voxel,uint tag) {
    return voxel|512u|(world_plant_plane(tag)<<10)|((tag&1)<<11);
}
float3 world_plant_voxel_normal(uint voxel) {
    return world_plant_normal(6+((voxel>>10)&1)*2+((voxel>>11)&1));
}
#endif
