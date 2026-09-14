#include "../Materials/MaterialTypes.slang"
StructuredBuffer<uint> blocks;
StructuredBuffer<int4> columnInfo;
RWStructuredBuffer<uint4> faces;
RWStructuredBuffer<uint> arguments;
StructuredBuffer<AtlasBlockMaterial> blockMaterials;
uniform uint emitFaces;
#include "WorldVisibility.slang"
uint blockAt(int3 p,int height) {
    if (p.x< -1 || p.x>32 || p.z< -1 || p.z>32 || p.y<0 || p.y>=height) return 0;
    return blocks[p.x+1+34*(p.y+height*(p.z+1))];
}
#include "WorldFluidMesh.slang"
#include "WorldPatches.slang"
RWStructuredBuffer<uint> patches;
void appendFace(int3 world,uint material,uint direction,uint pass,int3 local,int height,uint2 extent) {
    uint destination,patchDestination,patchCount=world_patch_count(extent);
    InterlockedAdd(arguments[pass*4+1],1,destination);
    InterlockedAdd(arguments[20+pass*4+1],patchCount,patchDestination);
    if (emitFaces!=0)
        {
        uint index=arguments[pass*4+3]+destination;
        faces[index]=uint4(asuint(world),material|(direction<<16)|((extent.x-1)<<20)|((extent.y-1)<<25));
        if(pass>=3) fluidFaces[index-arguments[3*4+3]]=originalFluidFace(material,local,height);
        uint patchBase=arguments[20+pass*4+3]+patchDestination;
        for(uint ordinal=0;ordinal<patchCount;++ordinal) patches[patchBase+ordinal]=(index<<6)|ordinal;
    }
}
#include "WorldGreedy.slang"
[shader("compute")]
[numthreads(32,1,1)]
void main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    int4 info=columnInfo[0];
    uint planeGroups=6*32*((uint(info.w)+31)/32);
    if(group.x<planeGroups) { greedyPlane(group.x,lane,info);return; }
    uint index=(group.x-planeGroups)*32+lane;
    if (index>=uint(32*32*info.w)) return;
    int3 p=int3(index%32,(index/32)%info.w,index/(32*info.w));
    uint material=blockAt(p,info.w); if (material==0) return;
    AtlasBlockMaterial data=blockMaterials[material];
    // Original procedural cloud owner does not mesh catalog cloud blocks.
    if ((data.flags&128)!=0) return;
    int3 world=p+int3(info.x*32,info.z,info.y*32);
    uint pass=meshClass(data);
    if(greedyCube(data)) return;
    if (pass==1) {
        // Four opposing crossed flower faces, six thin torch faces.
        uint count=(data.flags&32)!=0?6:4;
        for (uint face=0;face<count;++face) appendFace(world,material,6+face,pass,p,info.w,uint2(1));
        return;
    }
    const int3 directions[6]={int3(-1,0,0),int3(1,0,0),int3(0,-1,0),int3(0,1,0),int3(0,0,-1),int3(0,0,1)};
    for (uint direction=0;direction<6;++direction) {
        uint neighbor=blockAt(p+directions[direction],info.w);
        bool visible=cubeFaceVisible(material,neighbor);
        if ((data.flags&4)!=0) {
            uint other=blockMaterials[neighbor].flags;
            bool sameFluid=(other&4)!=0 && (other&512)==(data.flags&512);
            visible=!sameFluid && (other&1024)==0;
        }
        if (visible) appendFace(world,material,direction,pass,p,info.w,uint2(1));
    }
}
