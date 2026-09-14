#include "DDGITypes.slang"
StructuredBuffer<DDGIControl> ddgiControls;
StructuredBuffer<uint> ddgiSelection;
StructuredBuffer<DDGIRay> ddgiRays;
RWStructuredBuffer<DDGIProbe> ddgiProbes;
RWStructuredBuffer<float4> ddgiIrradiance;
RWStructuredBuffer<float2> ddgiDistance;
uniform uint ddgiUpdateCount;
groupshared DDGIProbe previous;
groupshared uint irradianceDelta;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    if(group.x>=ddgiUpdateCount)return;
    uint index=ddgiSelection[group.x],rayBase=group.x*ddgiFrame.y;
    if(ddgiRays[rayBase].radianceDistance.w < -ddgiParameters.z)return;
    DDGIControl control=ddgiControls[index];
    if(lane==0) {previous=ddgiProbes[index];irradianceDelta=0;}
    GroupMemoryBarrierWithGroupSync();
    bool reset=previous.metadata.x!=control.version||previous.metadata.w==0;
    bool changed=control.refreshFrame>previous.metadata.y;
    float history=reset?0:(changed?min(ddgiParameters.y,.8):ddgiParameters.y);
    uint irradianceCount=ddgiFrame.z*ddgiFrame.z;
    for(uint texel=lane;texel<irradianceCount;texel+=64) {
        float3 direction=ddgi_oct_decode((float2(texel%ddgiFrame.z,texel/ddgiFrame.z)+.5)/ddgiFrame.z);
        float3 sum=0;float weightSum=0;
        for(uint rayIndex=ddgi_fixed_rays();rayIndex<ddgiFrame.y;++rayIndex) {
            DDGIRay ray=ddgiRays[rayBase+rayIndex];
            if(ray.radianceDistance.w<0)continue;
            float weight=max(dot(direction,ray.directionHit.xyz),0);
            sum+=ray.radianceDistance.rgb*weight;weightSum+=weight;
        }
        // Cosine-normalized quadrature integrates a constant environment to pi*L.
        float3 irradiance=3.14159265359*sum/max(weightSum,1e-5);
        uint address=index*irradianceCount+texel;float3 old=reset?float3(0):ddgiIrradiance[address].rgb;
        float delta=length(irradiance-old)/max(length(old),.05);
        // Bright stochastic hits must not switch the temporal filter to 50%.
        // Respond quickly to darkening, while filtering isolated bright samples.
        float hysteresis=length(irradiance)<length(old)*.5?min(history,.8):history;
        ddgiIrradiance[address]=float4(lerp(irradiance,old,hysteresis),1);
        InterlockedMax(irradianceDelta,asuint(delta));
    }
    uint distanceCount=ddgiFrame.w*ddgiFrame.w;
    for(uint texel=lane;texel<distanceCount;texel+=64) {
        float3 direction=ddgi_oct_decode((float2(texel%ddgiFrame.w,texel/ddgiFrame.w)+.5)/ddgiFrame.w);
        float2 sum=0;float weightSum=0;
        for(uint rayIndex=ddgi_fixed_rays();rayIndex<ddgiFrame.y;++rayIndex) {
            DDGIRay ray=ddgiRays[rayBase+rayIndex];
            float weight=pow(max(dot(direction,ray.directionHit.xyz),0),32);
            // Visibility is local to neighboring probes. Distant sky misses
            // otherwise inflate variance enough to transmit through voxel walls.
            float distance=min(abs(ray.radianceDistance.w),ddgiParameters.x*2.598076211);
            sum+=float2(distance,distance*distance)*weight;weightSum+=weight;
        }
        uint address=index*distanceCount+texel;
        float2 moments=sum/max(weightSum,1e-7);
        ddgiDistance[address]=lerp(moments,reset?moments:ddgiDistance[address],history);
    }
    GroupMemoryBarrierWithGroupSync();
    if(lane!=0)return;
    DDGIProbe probe=previous;
    if(previous.metadata.x!=control.version)probe=(DDGIProbe)0;
    uint backfaces=0;float nearestBack=ddgiParameters.z,nearestFront=ddgiParameters.z;
    float3 backDirection=0,frontDirection=0;
    for(uint rayIndex=0;rayIndex<ddgi_fixed_rays();++rayIndex) {
        DDGIRay ray=ddgiRays[rayBase+rayIndex];float distance=ray.radianceDistance.w;
        if(distance<0) {
            ++backfaces;if(-distance<nearestBack) {nearestBack=-distance;backDirection=ray.directionHit.xyz;}
        } else if(ray.directionHit.w!=0&&distance<nearestFront) {nearestFront=distance;frontDirection=ray.directionHit.xyz;}
    }
    bool embedded=backfaces*4>ddgi_fixed_rays();
    float clearance=ddgiParameters.x*.1;float3 movement=0;
    if(embedded)movement=backDirection*(nearestBack+clearance);
    else if(nearestFront<clearance)movement=-frontDirection*(clearance-nearestFront);
    float3 offset=probe.offset.xyz+movement;float limit=ddgiParameters.x*.45;
    offset*=min(1,limit/max(length(offset),1e-6));
    bool relocated=length(offset-probe.offset.xyz)>.01;
    probe.offset.xyz=offset;
    probe.metadata.x=control.version;probe.metadata.y=ddgiFrame.x;
    probe.metadata.z=!reset&&!changed&&!relocated&&asfloat(irradianceDelta)<.025?probe.metadata.z+1:0;
    probe.metadata.w=(!embedded&&!relocated)?1:0;
    probe.offset.w=embedded&&!relocated?1:(probe.metadata.z>=8?2:0);
    ddgiProbes[index]=probe;
}
