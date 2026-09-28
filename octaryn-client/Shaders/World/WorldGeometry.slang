#ifndef OCTARYN_WORLD_GEOMETRY_SLANG
#define OCTARYN_WORLD_GEOMETRY_SLANG
#include "WorldSprites.slang"
#include "WorldPatches.slang"
struct WorldGeometryVertex {
    float3 offset;
    float2 uv;
    uint voxel;
    uint flags;
};
WorldGeometryVertex world_geometry_vertex(uint vertex,uint4 face,float2 coordinate) {
    uint direction=(face.w>>16)&15;
    float2 extent=float2(((face.w>>20)&31)+1,((face.w>>25)&31)+1);
    AtlasBlockMaterial material=blockMaterials[face.w&65535];
    const uint catalogDirections[6]={3,2,5,4,1,0};
    bool sprite=direction>=6;
    uint oldDirection=sprite?direction-6:catalogDirections[direction];
    const uint meshDirections[6]={5,4,1,0,3,2};
    uint layer=material.layers[sprite?meshDirections[oldDirection]:direction];
    float2 c=world_quad_corner(vertex);
    float2 scaled=coordinate;
    float3 offset;
    if (oldDirection==0) offset=float3(scaled.x,scaled.y,1);
    else if (oldDirection==1) offset=float3(extent.x-scaled.x,scaled.y,0);
    else if (oldDirection==2) offset=float3(1,scaled.y,extent.x-scaled.x);
    else if (oldDirection==3) offset=float3(0,scaled.y,scaled.x);
    else if (oldDirection==4) offset=float3(scaled.x,1,extent.y-scaled.y);
    else offset=float3(scaled.x,0,scaled.y);
    WorldGeometryVertex output;
    if (sprite) original_sprite(oldDirection,layer,c,offset,output.uv);
    else output.uv=oldDirection<4?float2(scaled.x,-scaled.y):scaled;
    output.offset=offset;
    output.voxel=oldDirection|(layer<<3)|((material.flags&2)!=0?256:0);
    output.flags=material.flags;
    return output;
}
WorldGeometryVertex world_geometry_corner(uint vertex,uint4 face) {
    float2 extent=float2(((face.w>>20)&31)+1,((face.w>>25)&31)+1);
    return world_geometry_vertex(vertex,face,world_quad_corner(vertex)*extent);
}
#endif
