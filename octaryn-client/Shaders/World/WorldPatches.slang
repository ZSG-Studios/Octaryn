#ifndef OCTARYN_WORLD_PATCHES_SLANG
#define OCTARYN_WORLD_PATCHES_SLANG
uint world_patch_count(uint2 extent) {
    return min(extent.x*extent.y,extent.x+extent.y);
}
float2 world_quad_corner(uint vertex) {
    const float2 corners[6]={float2(0,0),float2(1,0),float2(1,1),float2(0,0),float2(1,1),float2(0,1)};
    return corners[vertex];
}
float2 world_perimeter_point(uint2 extent,uint segment) {
    uint perimeter=2*(extent.x+extent.y);
    // The final fan endpoint is the only caller that reaches the perimeter.
    if(segment==perimeter) segment=0;
    if(segment<extent.x) return float2(segment,0);
    segment-=extent.x;
    if(segment<extent.y) return float2(extent.x,segment);
    segment-=extent.y;
    if(segment<extent.x) return float2(extent.x-segment,extent.y);
    segment-=extent.x;
    return float2(0,extent.y-segment);
}
float2 world_patch_corner(uint2 extent,uint ordinal,uint vertex) {
    // For positive extents, area<=width+height permits only strips and 2x2.
    if(extent.x==1) return float2(0,ordinal)+world_quad_corner(vertex);
    if(extent.y==1) return float2(ordinal,0)+world_quad_corner(vertex);
    if(extent.x==2 && extent.y==2)
        return float2(ordinal&1,ordinal>>1)+world_quad_corner(vertex);
    // Two consecutive perimeter segments per patch, with the original winding.
    // Every external edge is exactly one voxel long; adjacent rectangles agree.
    if(vertex==0 || vertex==3) return float2(extent)*.5;
    uint endpoint=vertex==1?0:vertex==5?2:1;
    return world_perimeter_point(extent,ordinal*2+endpoint);
}
#endif
