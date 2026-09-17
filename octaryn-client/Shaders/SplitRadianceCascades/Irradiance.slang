#include "Sample.slang"
// Probe-level Lambertian pre-integration: 36 cosine-weighted irradiance bins so
// pixel shading never gathers directional radiance per direction.
[shader("compute")][numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    id.x=src_index(id);
    if(id.x>=srcCounts.y)return;
    SrcProbe p=srcProbes[id.x];
    uint base=id.x*36u;
    if(p.lifetime==0 || src_probe_cascade(id.x)!=0) {
        for(uint b=0;b<36u;++b)srcOct[base+b]=float4(0,0,0,-1);
        return;
    }
    SrcCascade c=srcCascades[0];
    for(uint bin=0;bin<36u;++bin) {
        float3 n=src_oct_direction(bin);
        float3 sum=0;float weight=0;
        for(uint d=0;d<c.directions;++d) {
            float4 value=srcRadiance[c.directionOffset+id.x*c.directions+d];
            if(value.w<0 || !all(isfinite(value.xyz)))continue;
            float cosine=max(dot(n,src_bin_direction(d,0)),0);
            if(cosine<=0)continue;
            // Equal-area bins carry uniform solid angle; 2*pi/total normalizes.
            sum+=value.xyz*cosine;weight+=cosine;
        }
        // Equal-area bins over the full sphere: domega = 4*pi/directions.
        srcOct[base+bin]=weight>0?float4(sum*(4.0*SRC_PI/float(c.directions)),1):float4(0,0,0,-1);
    }
}
