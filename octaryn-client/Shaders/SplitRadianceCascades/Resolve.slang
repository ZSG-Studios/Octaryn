#include "HitRadiance.slang"
// Resolve cost scales with missing detail. Pixels whose surface-facing sparse
// donors fully cover their trilinear neighborhood take the pre-integrated C0
// irradiance directly; only pixels whose facing donors lack coverage run the
// short-range C-1 contact DDA hemisphere, as a stratified 8-bin subset whose
// cosine-weighted ratio normalization compensates for the direction count.
// Probe rays only cover the probe's own surface hemisphere, so a donor whose
// stored normal faces away from the pixel normal cannot hold oct data for it
// (its polar bins on the far side receive no deposits). Those donors still
// contribute irradiance where their bins are valid, but they are excluded from
// the coverage gate; otherwise every wall/floor junction pixel would run the
// contact hemisphere forever.
static const float SRC_DONOR_COVERAGE=0.98;
static const uint SRC_CONTACT_SAMPLES=8;
static const uint SRC_CONTACT_SHADOW_STEPS=1024;
struct SrcDonors {uint indices[8];float weights[8];float facing[8];};
struct SrcGather {float3 irradiance;bool valid;float coverage;};
SrcDonors src_donors(float3 position,float3 normal,uint lod) {
    SrcDonors donors;int3 base;float3 f;src_spatial(position,lod,0,base,f);
    for(uint i=0;i<8;++i) {
        donors.indices[i]=SRC_INVALID;donors.weights[i]=0;donors.facing[i]=0;
        int3 o=int3(i&1,(i>>1)&1,(i>>2)&1);float3 t=lerp(1-f,f,float3(o));
        float mass=t.x*t.y*t.z;
        if(mass*256.0<1.0)continue; // Negligible corner: not worth a visibility trace.
        uint probe=src_find(base+o,lod,0);if(probe==SRC_INVALID)continue;
        if(!src_visible(position,src_probe_surface(probe,0))) {src_stats_blocked();continue;}
        SrcProbe p=srcProbes[probe];
        donors.indices[i]=probe;donors.weights[i]=mass*max(p.quality,.001);
        donors.facing[i]=dot(p.normal.xyz,normal)>0?mass:0;
    }
    return donors;
}
SrcGather src_gather_oct(SrcDonors donors,float3 normal) {
    SrcGather gather;gather.irradiance=0;gather.valid=false;gather.coverage=-1;
    float3 sum=0;float weight=0,validFacing=0,foundFacing=0;
    for(uint i=0;i<8;++i) {
        uint probe=donors.indices[i];if(probe==SRC_INVALID)continue;
        foundFacing+=donors.facing[i];
        float4 value=src_oct_sample(probe,normal);if(value.w<0)continue;
        sum+=value.xyz*donors.weights[i];weight+=donors.weights[i];
        validFacing+=donors.facing[i];
    }
    gather.valid=weight>0;
    if(gather.valid)gather.irradiance=sum/weight;
    if(foundFacing>0)gather.coverage=validFacing/foundFacing;
    return gather;
}
SrcGather src_blend_gather(SrcGather near,SrcGather far,float nearWeight) {
    SrcGather blended;
    float nearShare=near.valid?nearWeight:0,farShare=far.valid?1-nearWeight:0;
    float total=nearShare+farShare;
    blended.valid=total>0;
    blended.irradiance=total>0?(max(near.irradiance,0)*nearShare+max(far.irradiance,0)*farShare)/total:float3(0,0,0);
    float nearCov=near.coverage>=0?nearWeight:0,farCov=far.coverage>=0?1-nearWeight:0;
    float covTotal=nearCov+farCov;
    blended.coverage=covTotal>0?(max(near.coverage,0)*nearCov+max(far.coverage,0)*farCov)/covTotal:-1;
    return blended;
}
// Stratified subset of the C0 bin grid: rows are elevation bands, columns are
// azimuth; per-row samples stay evenly spread and rotate per pixel/frame.
uint src_contact_bin(uint k,uint samples,uint resolution,uint columnRotation) {
    uint columns=2*resolution,row,samplesPerRow;
    if(samples>=resolution) {samplesPerRow=samples/resolution;row=k/samplesPerRow;}
    else {samplesPerRow=1;row=(k*resolution)/samples;}
    uint column=(columnRotation+row+(k%samplesPerRow)*(columns/samplesPerRow))%columns;
    return row*columns+column;
}
[shader("compute")][numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=srcExtent.xy))return;float3 position,normal;
    if(!src_surface(id.xy,position,normal)) {srcLighting[id.xy]=0;return;}
    float3 start=src_surface_start(position,normal);uint lod=src_lod(position);
    SrcDonors low=src_donors(start,normal,lod),high=low;
    float lowWeight=src_lod_weight(position,lod);
    if(lowWeight<1)high=src_donors(start,normal,lod+1);
    SrcGather donor=src_gather_oct(low,normal);
    if(lowWeight<1)donor=src_blend_gather(donor,src_gather_oct(high,normal),lowWeight);
    // Donor-complete pixels already carry full C0 irradiance; this is the exact
    // value the contact loop below produces when every direction misses.
    if(donor.coverage>=SRC_DONOR_COVERAGE) {
        srcLighting[id.xy]=float4(donor.irradiance*SRC_PI,1);src_stats_flush();return;
    }
    float3 irradiance=0;float coverage=0,cosine=0;
    uint rng=src_hash(id.x+id.y*srcExtent.x+srcExtent.z*1361);
    // Contact detail below C0 spacing stays a short voxel DDA hemisphere query.
    float contact=srcIntervals.y*srcIntervals.x*exp2(float(lod));
    uint resolution=srcCascades[0].resolution,directions=srcCascades[0].directions;
    uint samples=min(SRC_CONTACT_SAMPLES,directions);
    uint columnRotation=uint(src_random(rng)*float(2*resolution))%(2*resolution);
    for(uint k=0;k<samples;++k) {
        float3 direction=src_bin_direction(src_contact_bin(k,samples,resolution,columnRotation),0);
        float w=max(dot(normal,direction),0);if(w<=0)continue;cosine+=w;
        SrcHit hit=(SrcHit)0;hit.status=VOXEL_TRACE_MISS;
        if(contact>0)hit=src_trace_budget(start,direction,0,contact,SRC_CONTACT_SHADOW_STEPS);
        if(hit.status==VOXEL_TRACE_CANDIDATE) {
            irradiance+=src_hit_radiance(hit,rng,SRC_CONTACT_SHADOW_STEPS)*w;coverage+=w;continue;
        }
        if(hit.status!=VOXEL_TRACE_MISS)continue;
        if(donor.valid) {irradiance+=donor.irradiance*w;coverage+=w;}
    }
    // Zero-count directions do not enter normalization (paper section 5).
    srcLighting[id.xy]=float4(coverage>0?irradiance*(SRC_PI/coverage):float3(0),coverage/max(cosine,1e-6));
    src_stats_flush();
}
