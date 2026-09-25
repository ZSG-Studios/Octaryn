// Visibility and class order ported from original world/chunks/build_mesh.cpp.
uint meshClass(AtlasBlockMaterial material) {
    if ((material.flags&2)!=0) return 1;
    if ((material.flags&16)!=0) return 0;
    if ((material.flags&4)!=0) return (material.flags&512)!=0?4:3;
    return 2;
}
bool cubeFaceVisible(uint block,uint neighbor) {
    if (neighbor==0) return true;
    uint flags=blockMaterials[block].flags,other=blockMaterials[neighbor].flags;
    if ((flags&64)!=0) return (other&64)==0 && (other&1)==0;
    if ((flags&128)!=0 && (other&128)!=0) return false;
    if ((flags&256)!=0 && (other&256)!=0) return true;
    if ((other&2)!=0) return true;
    return (flags&16)!=0 && (other&1)==0;
}
