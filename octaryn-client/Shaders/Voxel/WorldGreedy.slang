#ifndef OCTARYN_WORLD_GREEDY_SLANG
#define OCTARYN_WORLD_GREEDY_SLANG
// Included after WorldFaces' bindings, visibility and appendFace declarations.
groupshared uint greedyKeys[32*32];
groupshared uint greedyRows[32];
bool greedyCube(AtlasBlockMaterial material) {
    // Cutout coverage retains the original unit triangulation.
    return meshClass(material)==0 && (material.flags&1)!=0 && (material.flags&(2|4|128))==0;
}
// Minimum voxel anchor; U/V extend +Z/+Y, +X/+Z, or +X/+Y respectively.
int3 greedyPosition(uint direction,uint slice,uint band,uint u,uint v) {
    if(direction<2) return int3(slice,band+v,u);
    if(direction<4) return int3(u,band+slice,v);
    return int3(u,band+v,slice);
}
void greedyPlane(uint group,uint row,int4 info) {
    uint slice=group%32,direction=(group/32)%6,band=(group/(32*6))*32;
    const int3 directions[6]={int3(-1,0,0),int3(1,0,0),int3(0,-1,0),int3(0,1,0),int3(0,0,-1),int3(0,0,1)};
    uint mask=0;
    for(uint u=0;u<32;++u) {
        int3 p=greedyPosition(direction,slice,band,u,row);
        uint material=blockAt(p,info.w),key=0;
        if(material!=0 && greedyCube(blockMaterials[material]) &&
            cubeFaceVisible(material,blockAt(p+directions[direction],info.w))) key=material;
        greedyKeys[row*32+u]=key;
        if(key!=0) mask|=1u<<u;
    }
    greedyRows[row]=mask;
    GroupMemoryBarrierWithGroupSync();
    if(row!=0) return;
    uint planeCount=0,planePatches=0;
    // One lane consumes a bounded tile; no subgroup width or scheduling assumptions.
    for(uint v=0;v<32;++v) while(greedyRows[v]!=0) {
        uint u=firstbitlow(greedyRows[v]),material=greedyKeys[v*32+u],width=1,height=1;
        while(u+width<32) {
            if((greedyRows[v]&(1u<<(u+width)))==0 || greedyKeys[v*32+u+width]!=material) break;
            ++width;
        }
        uint span=(0xffffffffu>>(32-width))<<u;
        while(v+height<32) {
            if((greedyRows[v+height]&span)!=span) break;
            bool equal=true;
            for(uint x=0;x<width;++x) if(greedyKeys[(v+height)*32+u+x]!=material) { equal=false;break; }
            if(!equal) break;
            ++height;
        }
        for(uint y=0;y<height;++y) greedyRows[v+y]&=~span;
        int3 p=greedyPosition(direction,slice,band,u,v);
        if(emitFaces!=0)
            appendFace(p+int3(info.x*32,info.z,info.y*32),material,direction,0,p,info.w,uint2(width,height));
        else { ++planeCount;planePatches+=world_patch_count(uint2(width,height)); }
    }
    // Count-only needs one global atomic per occupied plane, not per rectangle.
    if(emitFaces==0 && planeCount!=0) {
        InterlockedAdd(arguments[1],planeCount);
        InterlockedAdd(arguments[21],planePatches);
    }
}
#endif
