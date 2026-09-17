#include "Sample.slang"
[shader("compute")][numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    SrcCascade c=srcCascades[srcLevel];if(id.x>=c.capacity*c.directions)return;
    uint probe=c.probeOffset+id.x/c.directions,dir=id.x%c.directions,address=c.directionOffset+id.x;
    SrcProbe p=srcProbes[probe];if(p.lifetime==0)return;
    if(dir==0) {srcProbes[probe].quality=max(p.quality,float(p.reserved));srcProbes[probe].reserved=0;}
    float weight=float(srcWeights[address]);
    if(weight<1e-6) {srcRadiance[address]=float4(0,0,0,-1);return;}
    float4 near=float4(srcFluence[address])/65536.0/weight;
    float4 far=0;float total=0;
    if(srcLevel+1<srcCounts.x && near.w>0) {
        // Probe centers map to .25/.75 positions in the next spatial grid.
        float3 direction=src_bin_direction(dir,srcLevel);
        SrcLinks links=srcLinks[probe];
        for(uint i=0;i<8;++i) {
            uint parent=links.indices[i/4][i%4];
            if(parent==SRC_INVALID)continue;
            float w=links.weights[i/4][i%4]*max(srcProbes[parent].quality,.001);
            float4 v=src_angular(parent,srcLevel+1,direction,false);if(v.w<0)continue;far+=v*w;total+=w;
        }
    }
    if(near.w>0 && total==0) {srcRadiance[address]=float4(0,0,0,-1);return;}
    if(total>0)far/=total;
    srcRadiance[address]=float4(near.xyz+near.w*far.xyz,near.w*far.w);
}
