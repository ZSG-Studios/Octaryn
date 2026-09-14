#ifndef OCTARYN_WORLD_GEOMETRY_SLANG
#define OCTARYN_WORLD_GEOMETRY_SLANG
#include "WorldFluidTypes.slang"
#include "WorldSprites.slang"
#include "WorldPatches.slang"
struct WorldGeometryVertex {
    float3 offset;
    float2 uv;
    uint voxel;
    uint flags;
    float4 fluid;
    float fill;
};
WorldGeometryVertex world_geometry_vertex(uint vertex,uint4 face,float2 coordinate,FluidFace fluid) {
    uint direction=(face.w>>16)&15;
    float2 extent=float2(((face.w>>20)&31)+1,((face.w>>25)&31)+1);
    AtlasBlockMaterial material=blockMaterials[face.w&65535];
    const uint catalogDirections[6]={3,2,5,4,1,0};
    bool sprite=direction>=6;
    uint oldDirection=sprite?direction-6:catalogDirections[direction];
    const uint meshDirections[6]={5,4,1,0,3,2};
    uint layer=material.layers[sprite?meshDirections[oldDirection]:direction];
    float2 c=world_quad_corner(vertex);
    if((material.flags&4)!=0) {
        if(oldDirection==4 && abs(fluid.heights.z-fluid.heights.x)<abs(fluid.heights.w-fluid.heights.y)) {
            const float2 alternate[6]={float2(0,0),float2(1,0),float2(0,1),float2(1,0),float2(1,1),float2(0,1)};
            c=alternate[vertex];
        }
    }
    float2 scaled=(material.flags&4)!=0?c*extent:coordinate;
    float3 offset;
    if (oldDirection==0) offset=float3(scaled.x,scaled.y,1);
    else if (oldDirection==1) offset=float3(extent.x-scaled.x,scaled.y,0);
    else if (oldDirection==2) offset=float3(1,scaled.y,extent.x-scaled.x);
    else if (oldDirection==3) offset=float3(0,scaled.y,scaled.x);
    else if (oldDirection==4) offset=float3(scaled.x,1,extent.y-scaled.y);
    else offset=float3(scaled.x,0,scaled.y);
    WorldGeometryVertex output;output.fluid=0;output.fill=1;
    if (sprite) original_sprite(oldDirection,layer,c,offset,output.uv);
    else output.uv=oldDirection<4?float2(scaled.x,-scaled.y):scaled;
    if ((material.flags&4)!=0) {
        float surface=lerp(lerp(fluid.heights.x,fluid.heights.y,offset.x),lerp(fluid.heights.w,fluid.heights.z,offset.x),offset.z);
        if(oldDirection<4 && fluid.flow.w!=0) surface=1;
        offset.y*=surface;output.fill=offset.y;output.fluid=float4(fluid.flow.xyz,float(material.fluidLevel));
        if(oldDirection==4 && fluid.flow.z>0) {
            float angle=atan2(-fluid.flow.y,fluid.flow.x)-1.57079632679;
            float sn=sin(angle)*.25,cs=cos(angle)*.25;
            float2 axis=c*2-1;output.uv=.5+float2(axis.x*cs+axis.y*sn,-axis.x*sn+axis.y*cs);
        }
    }
    output.offset=offset;
    output.voxel=oldDirection|(layer<<3)|((material.flags&2)!=0?256:0);
    output.flags=material.flags;
    return output;
}
WorldGeometryVertex world_geometry_corner(uint vertex,uint4 face,FluidFace fluid) {
    float2 extent=float2(((face.w>>20)&31)+1,((face.w>>25)&31)+1);
    return world_geometry_vertex(vertex,face,world_quad_corner(vertex)*extent,fluid);
}
#endif
