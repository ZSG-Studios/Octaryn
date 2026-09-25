#ifndef OCTARYN_BLOCK_TRANSPORT_WORLD_ADMISSION
#define OCTARYN_BLOCK_TRANSPORT_WORLD_ADMISSION
#include "Reflectance.slang"
#include "../Voxel/WorldGeometry.slang"
#include "../Voxel/WorldSurfaceKey.slang"

uint bt_world_surface_count(uint4 face) {
    uint direction=(face.w>>16)&15;
    AtlasBlockMaterial material=blockMaterials[face.w&65535];
    if(direction<6) {
        if(!world_surface_transport_supported(material.flags))return 0;
        if((material.flags&1)==0 && !bt_cube_mask_ready(material.layers[direction]))return 0;
        return (((face.w>>20)&31)+1)*(((face.w>>25)&31)+1);
    }
    // The mesh submits each crossed plane twice with identical geometry/UVs.
    if(direction!=6 && direction!=8)return 0;
    WorldGeometryVertex vertex=world_geometry_corner(0,face,(FluidFace)0);
    if(!world_crossed_plant(material.flags,vertex.voxel,direction))return 0;
    uint tag=direction|(get_index(vertex.voxel)<<4);
    return bt_plant_mask_ready(int4(asint(face.xyz),int(tag)))?2:0;
}

bool bt_world_surface(uint4 face,uint ordinal,out int4 key,out float4 reflectance) {
    key=int4(0,0,0,-1);reflectance=0;
    uint count=bt_world_surface_count(face);
    if(ordinal>=count)return false;
    uint direction=(face.w>>16)&15;
    WorldGeometryVertex vertex=world_geometry_corner(0,face,(FluidFace)0);
    if(direction<6) {
        uint width=((face.w>>20)&31)+1;
        int u=int(ordinal%width),v=int(ordinal/width);
        int3 offset=direction<2?int3(0,v,u):direction<4?int3(u,0,v):int3(u,v,0);
        int3 anchor=asint(face.xyz);
        if(any(anchor>int3(2147483647)-offset))return false;
        key=int4(anchor+offset,int(direction));
    } else {
        uint tag=(direction+ordinal)|(get_index(vertex.voxel)<<4);
        key=int4(asint(face.xyz),int(tag));
    }
    return bt_surface_material(key,vertex.voxel,vertex.flags,reflectance);
}
#endif
