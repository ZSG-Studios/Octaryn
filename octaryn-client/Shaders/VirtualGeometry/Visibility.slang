#ifndef OCTARYN_GEOMETRY_VISIBILITY_SLANG
#define OCTARYN_GEOMETRY_VISIBILITY_SLANG
#include "Geometry.slang"
[__AttributeUsage(_AttributeTargets.Var)]
struct rootAttribute {};
[root] RWStructuredBuffer<uint64_t> geometryVisibility;
// Reversed positive float ordering: nearest depth wins, then smallest stable ID.
void geometry_commit(uint2 pixel,float depth,uint selected,uint triangle,bool front) {
    if(any(pixel>=geometryExtent.xy) || !isfinite(depth) || depth<0 || depth>1)return;
    // Selection compaction order may vary between frames; ties use the cooked ID.
    uint id=((geometrySelected[selected].x*128+triangle+1)<<1)|uint(!front);
    uint64_t key=(uint64_t(~asuint(max(depth,0.0)))<<32)|uint64_t(~id);
    uint64_t previous;InterlockedMax(geometryVisibility[pixel.y*geometryExtent.x+pixel.x],key,previous);
}
[shader("compute")]
[numthreads(256,1,1)]
void clear_main(uint id:SV_DispatchThreadID) {
    if(id<geometryExtent.x*geometryExtent.y)geometryVisibility[id]=0;
}
#endif
