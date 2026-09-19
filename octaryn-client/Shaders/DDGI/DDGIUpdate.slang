#include "DDGIIrradiance.slang"
StructuredBuffer<DDGIControl> ddgiControls;
StructuredBuffer<uint> ddgiSelection;
StructuredBuffer<DDGIRay> ddgiRays;

RWStructuredBuffer<DDGIProbe> ddgiProbes;
RWStructuredBuffer<float4> ddgiIrradiance;
RWStructuredBuffer<float2> ddgiDistance;
RWStructuredBuffer<float> ddgiVariability;
uniform uint ddgiUpdateCount;
groupshared DDGIProbe previous;
groupshared uint irradianceDelta;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    if(group.x>=ddgiUpdateCount)return;
    uint packed=ddgiSelection[group.x],index=ddgi_slot_index(packed);
    uint rays=ddgi_slot_rays(packed),rayBase=group.x*ddgiFrame.y;
    if(ddgiRays[rayBase].radianceDistance.w < -ddgiParameters.z)return;
    DDGIControl control=ddgiControls[index];
    if(lane==0) {previous=ddgiProbes[index];irradianceDelta=0;}
    GroupMemoryBarrierWithGroupSync();
    bool reset=previous.metadata.x!=control.version||previous.metadata.w==0||previous.metadata.y==0;
    bool changed=control.refreshFrame>previous.metadata.y;
    bool geometryChanged=changed&&(control.padding.z&4u)==0;
    // A hard removal snaps dark: the recursive marker already rejected the old
    // bounce, and the remaining ambient is stable, so no blend tail may linger.
    bool removed=changed&&(control.padding.z&2u)!=0;
    bool geometryLocked=ddgi_geometry_locked(control,previous,reset);
    // A source change leans on the bounded reactive policy for several
    // observations; only a genuine occlusion change discards irradiance at once.
    uint reactive=reset?0u:(changed?4u:(previous.metadata.z>>16));
    // Retain history per ray observation; idle frames do not add samples.
    float history=reset||geometryChanged||removed?0:ddgi_history_retention(ddgiParameters.y,previous.metadata.w,reactive,reset);
    float geometryHistory=geometryLocked?ddgi_history_retention(ddgiParameters.y,previous.metadata.w,0,false):0;
    uint irradianceCount=ddgiFrame.z*ddgiFrame.z;
    for(uint texel=lane;texel<irradianceCount;texel+=64) {
        float3 direction=ddgi_oct_decode((float2(texel%ddgiFrame.z,texel/ddgiFrame.z)+.5)/ddgiFrame.z);
        float3 irradiance=ddgi_integrate_irradiance(ddgiRays,rayBase,ddgi_fixed_rays(),rays,direction);
        uint address=index*irradianceCount+texel;float3 old=reset?float3(0):ddgiIrradiance[address].rgb;
        float delta=length(irradiance-old)/max(length(old),.05);
        ddgiIrradiance[address]=float4(ddgi_blend_irradiance(irradiance,old,history),1);
        InterlockedMax(irradianceDelta,asuint(delta));
    }
    uint distanceCount=ddgiFrame.w*ddgiFrame.w;
    for(uint texel=lane;texel<distanceCount;texel+=64) {
        float3 direction=ddgi_oct_decode((float2(texel%ddgiFrame.w,texel/ddgiFrame.w)+.5)/ddgiFrame.w);
        float2 sum=0;float weightSum=0;
        // Geometry moments use deterministic spherical quadrature. The lighting
        // rays still rotate for temporal irradiance sampling across updates.
        for(uint rayIndex=0;rayIndex<ddgi_fixed_rays();++rayIndex) {
            DDGIRay ray=ddgiRays[rayBase+rayIndex];
            float weight=pow(max(dot(direction,ray.directionHit.xyz),0),16);
            // Visibility is local to neighboring probes. Distant sky misses
            // otherwise inflate variance enough to transmit through voxel walls.
            float distance=abs(ray.radianceDistance.w)*(ray.radianceDistance.w<0?.2:1);
            distance=min(distance,ddgiParameters.x*2.598076211);
            sum+=float2(distance,distance*distance)*weight;weightSum+=weight;
        }
        uint address=index*distanceCount+texel;
        float2 moments=sum/max(weightSum,1e-7);
        // Geometry publication is explicit and these directions are fixed:
        // old open-wall visibility must not survive a newly closed voxel wall.
        ddgiDistance[address]=lerp(moments,reset?moments:ddgiDistance[address],geometryHistory);
    }
    GroupMemoryBarrierWithGroupSync();
    if(lane!=0)return;
    DDGIProbe probe=previous;
    if(previous.metadata.x!=control.version)probe=(DDGIProbe)0;
    // Variability EMA: instantaneous max texel delta smoothed across updates.
    // Sleep requires both the instantaneous and the smoothed signal to settle.
    float variability=reset?asfloat(irradianceDelta):
        lerp(asfloat(irradianceDelta),ddgiVariability[index],.9);
    ddgiVariability[index]=variability;
    uint backfaces=0;float nearestBack=ddgiParameters.z,nearestFront=ddgiParameters.z;
    float3 backDirection=0,frontDirection=0;
    for(uint rayIndex=0;rayIndex<ddgi_fixed_rays();++rayIndex) {
        DDGIRay ray=ddgiRays[rayBase+rayIndex];float distance=ray.radianceDistance.w;
        if(distance<0) {
            ++backfaces;if(-distance<nearestBack) {nearestBack=-distance;backDirection=ray.directionHit.xyz;}
        } else if(ray.directionHit.w!=0&&distance<nearestFront) {nearestFront=distance;frontDirection=ray.directionHit.xyz;}
    }
    // The ray scene is immutable between geometry publications. A validated
    // location stays valid until an edit/stream publication requests a recheck.
    bool embedded=!geometryLocked&&backfaces*4>ddgi_fixed_rays();
    float clearance=ddgiParameters.x*.1;float3 movement=0;
    if(!geometryLocked) {
        if(embedded)movement=backDirection*(nearestBack+clearance);
        else if(nearestFront<clearance)movement=-frontDirection*(clearance-nearestFront);
    }
    float3 offset=probe.offset.xyz+movement;float limit=ddgiParameters.x*.45;
    offset*=min(1,limit/max(length(offset),1e-6));
    bool relocated=length(offset-probe.offset.xyz)>.01;
    probe.offset.xyz=offset;
    probe.metadata.x=control.version;probe.metadata.y=ddgiFrame.x;
    uint stable=!reset&&reactive==0&&!relocated&&asfloat(irradianceDelta)<.025&&variability<.05?
        min((previous.metadata.z&65535u)+1,65535u):0;
    probe.metadata.z=stable|((reactive>0?reactive-1:0)<<16);
    probe.metadata.w=(!embedded&&!relocated)?min(reset?1u:previous.metadata.w+1,65535u):0;
    probe.offset.w=embedded&&!relocated?1:(stable>=8?2:0);
    ddgiProbes[index]=probe;
}
