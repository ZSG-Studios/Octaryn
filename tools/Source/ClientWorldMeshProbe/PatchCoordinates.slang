#include "WorldPatches.slang"
StructuredBuffer<uint4> patchCases;
RWStructuredBuffer<uint> differences;
uniform uint caseCount;
// Frozen preoptimization arithmetic, intentionally separate from production's
// strip cases and single-wrap fan arithmetic. This is a bit-exact A/B oracle.
float2 previous_perimeter(uint2 extent,uint segment) {
    segment%=2*(extent.x+extent.y);
    if(segment<extent.x) return float2(segment,0);
    segment-=extent.x;
    if(segment<extent.y) return float2(extent.x,segment);
    segment-=extent.y;
    if(segment<extent.x) return float2(extent.x-segment,extent.y);
    segment-=extent.x;
    return float2(0,extent.y-segment);
}
float2 previous_corner(uint2 extent,uint ordinal,uint vertex) {
    if(extent.x*extent.y<=extent.x+extent.y)
        return float2(ordinal%extent.x,ordinal/extent.x)+world_quad_corner(vertex);
    uint corner=vertex%3;
    if(corner==0) return float2(extent)*.5;
    return previous_perimeter(extent,ordinal*2+vertex/3+corner-1);
}
[shader("compute")]
[numthreads(64,1,1)]
void patch_coordinates(uint3 index:SV_DispatchThreadID) {
    if(index.x>=caseCount) return;
    uint4 test=patchCases[index.x];
    float2 actual=world_patch_corner(test.xy,test.z,test.w);
    float2 expected=previous_corner(test.xy,test.z,test.w);
    differences[index.x]=any(asuint(actual)!=asuint(expected))?1:0;
}
