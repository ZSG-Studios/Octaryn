Texture2D<float4> previousRadiance;
Texture2D<float4> previousSurface;
Texture2D<float2> previousMoments;
RWTexture2D<uint> tiles;
uniform float4 dimensions;
groupshared uint unstablePixels;
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID,uint3 tile:SV_GroupID,uint lane:SV_GroupIndex) {
    if(lane==0)unstablePixels=0;
    GroupMemoryBarrierWithGroupSync();
    if(all(id.xy<uint2(dimensions.xy))) {
        float4 history=previousRadiance[id.xy];
        float roughness=previousSurface[id.xy].w;
        float2 moments=previousMoments[id.xy];
        float variance=max(0,moments.y-moments.x*moments.x)/max(.01,moments.x*moments.x);
        if(history.a<16 || roughness<.45 || variance>.025)
            InterlockedAdd(unstablePixels,1);
    }
    GroupMemoryBarrierWithGroupSync();
    // Per-pixel reprojection still validates every receiver before reuse.
    if(lane==0)tiles[tile.xy]=unstablePixels<=4?1u:0u;
}
