StructuredBuffer<uint> localTileCounts;
RWStructuredBuffer<uint> localTileLights;
uniform uint4 localTileSettings;
groupshared uint indices[256];
[shader("compute")][numthreads(256,1,1)]
void main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    uint tile=group.y*localTileSettings.x+group.x,count=localTileCounts[tile];
    if(count>localTileSettings.z||count<2)return;
    uint size=1;while(size<count)size*=2;
    indices[lane]=lane<count?localTileLights[tile*localTileSettings.z+lane]:0xffffffff;
    GroupMemoryBarrierWithGroupSync();
    // Atomic scatter order is arbitrary; sort once per tile, not per pixel.
    for(uint width=2;width<=size;width*=2)for(uint stride=width/2;stride>0;stride/=2) {
        uint other=lane^stride;
        if(lane<size&&other>lane) {
            bool ascending=(lane&width)==0;
            if((indices[lane]>indices[other])==ascending) {
                uint value=indices[lane];indices[lane]=indices[other];indices[other]=value;
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if(lane<count)localTileLights[tile*localTileSettings.z+lane]=indices[lane];
}
