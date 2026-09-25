#include "../../../octaryn-client/Shaders/Materials/PbrEnvironment.slang"
RWStructuredBuffer<float4> pbrResults;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint i=id.x;
    if(i==0) {
        pbrResults[i]=float4(pbr_environment_reflectance(float3(.04),1,.001),
            pbr_environment_reflectance(float3(.04),0,1).x);
        return;
    }
    float nv=(float(i%8)+.001)/8;
    float alpha=max(pow(float(i/8)/7,2),.0025);
    float3 view=float3(sqrt(1-nv*nv),0,nv);
    float3 h=pbr_visible_normal(view,alpha,float2(frac(float(i)*.61803398875),frac(float(i)*.754877666)));
    float3 light=reflect(-view,h);
    float weight=light.z>0?pbr_smith_g1(light.z,alpha*alpha):0;
    pbrResults[i]=float4(length(h),dot(view,h),h.z,weight);
}
