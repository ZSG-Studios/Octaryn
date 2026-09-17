#ifndef OCTARYN_VOXEL_FLUID_SURFACE
#define OCTARYN_VOXEL_FLUID_SURFACE
#include "VoxelTraceBuffers.slang"
#include "../Materials/MaterialSampling.slang"
#include "../Voxel/WorldFluidTypes.slang"
uniform uint voxelTraceMaterialCount;

bool voxel_trace_read_block(int3 chunk,int3 local,out uint material) {
    material=0;
    for(uint axis=0;axis<3;++axis) {
        int carry=local[axis]/32-(local[axis]%32<0?1:0);
        if((carry>0 && chunk[axis]>2147483647-carry) ||
           (carry<0 && chunk[axis]<(-2147483647-1)-carry))return false;
        chunk[axis]+=carry;local[axis]-=carry*32;
    }
    VoxelTraceBuffers world;uint token;uint state=world.chunk(chunk,token);
    if(state==VOXEL_CHUNK_UNKNOWN)return false;
    if(state==VOXEL_CHUNK_EMPTY)return true;
    uint2 known=world.knownY(token);
    if(local.y<int(known.x) || local.y>=int(known.y))return false;
    material=world.material(token,uint3(local));
    return material<voxelTraceMaterialCount;
}
uint voxel_fluid_block(int3 chunk,int3 local,inout bool known) {
    uint material;bool valid=voxel_trace_read_block(chunk,local,material);
    known=known && valid;return valid?material:0;
}
bool voxel_same_fluid(uint a,uint b) {
    uint x=blockMaterials[a].flags,y=blockMaterials[b].flags;
    return (x&4)!=0 && (y&4)!=0 && (x&512)==(y&512);
}
float voxel_fluid_sample(uint center,int3 chunk,int3 p,inout bool known) {
    uint block=voxel_fluid_block(chunk,p,known);
    if(!voxel_same_fluid(center,block))return (blockMaterials[block].flags&1024)!=0?-1:0;
    if(voxel_same_fluid(center,voxel_fluid_block(chunk,p+int3(0,1,0),known)))return 1;
    return float(clamp(8-int(blockMaterials[block].fluidLevel),1,8))/9;
}
float voxel_fluid_corner(uint block,int3 chunk,int3 p,int dx,int dz,inout bool known) {
    float self=voxel_fluid_sample(block,chunk,p,known);
    float x=voxel_fluid_sample(block,chunk,p+int3(dx,0,0),known);
    float z=voxel_fluid_sample(block,chunk,p+int3(0,0,dz),known);
    if(x>=1 || z>=1)return 1;
    float diagonal=-1;
    if(x>0 || z>0) {
        diagonal=voxel_fluid_sample(block,chunk,p+int3(dx,0,dz),known);
        if(diagonal>=1)return 1;
    }
    float4 values=float4(self,x,z,diagonal);
    float4 weights=select(values>=.8,10.0,select(values>=0,1.0,0.0));
    return dot(values,weights)/max(dot(weights,1.0),1.0);
}
// Same heights, weighting, flow and stacked-side policy as WorldFluidMesh.slang;
// queries read immutable material voxels instead of a mesh or BLAS publication.
FluidFace voxel_fluid_face(uint block,int3 chunk,int3 p,out bool known) {
    known=true;FluidFace result;
    result.heights.x=voxel_fluid_corner(block,chunk,p,-1,-1,known);
    result.heights.y=voxel_fluid_corner(block,chunk,p,1,-1,known);
    result.heights.z=voxel_fluid_corner(block,chunk,p,1,1,known);
    result.heights.w=voxel_fluid_corner(block,chunk,p,-1,1,known);
    float center=voxel_fluid_sample(block,chunk,p,known);float2 flow=0;
    const int2 offsets[4]={int2(1,0),int2(-1,0),int2(0,1),int2(0,-1)};
    for(uint i=0;i<4;++i) {
        int3 q=p+int3(offsets[i].x,0,offsets[i].y);
        uint neighbor=voxel_fluid_block(chunk,q,known);float pull=0;
        if(voxel_same_fluid(block,neighbor))pull=center-voxel_fluid_sample(block,chunk,q,known);
        else if((blockMaterials[neighbor].flags&1)==0 &&
                voxel_same_fluid(block,voxel_fluid_block(chunk,q-int3(0,1,0),known)))
            pull=center-(voxel_fluid_sample(block,chunk,q-int3(0,1,0),known)-8.0/9.0);
        flow+=float2(offsets[i])*pull;
    }
    float magnitude=length(flow);
    result.flow=magnitude>.0001?float4(flow/magnitude,clamp(floor(magnitude*15+.5),1,15),0):0;
    result.flow.w=voxel_same_fluid(block,voxel_fluid_block(chunk,p+int3(0,1,0),known))?1:0;
    return result;
}
#endif
