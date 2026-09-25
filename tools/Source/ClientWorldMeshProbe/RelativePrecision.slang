#include "WorldRaster.slang"
RWStructuredBuffer<float4> precisionVertices;
[shader("compute")]
[numthreads(1,1,1)]
void relative_precision(uint3 index:SV_DispatchThreadID) {
    // Same callable body used by the production vertex entry, without raster rounding.
    VertexOutput value=world_vertex(index.x%6,index.x/6);
    precisionVertices[index.x*2]=float4(value.relative,value.fill);
    precisionVertices[index.x*2+1]=value.position;
}
