#include "Cache.slang"
#include "Sampling.slang"
void bt_record_local_ray() {InterlockedAdd(btCounters[7],1);}
#include "LocalArea.slang"
#include "Work.slang"
#include "../Lighting/DiffuseEnvironment.slang"
StructuredBuffer<uint4> btLinks;
RWStructuredBuffer<float4> btDirect,btEnvironment;
uniform float4 btSun,btSky;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=btWork.y)return;
    uint row;if(!bt_work_row(id.x,row))return;
    BtSurface surface=btSurfaces[row];
    if(surface.state.x!=btFrameInfo.x || surface.state.z==BT_INVALID)return;
    uint batch=surface.state.y;
    btSurfaces[row].state.y=batch+1;
    uint random=bt_hash(bt_key_hash(surface.key)^bt_hash(batch));
    float3 normal=bt_normal(surface.key.w),position=bt_center(surface.key);
    if(!bt_surface_position(surface,batch,random,position)) {
        btDirect[row]=0;btEnvironment[row]=0;btSurfaces[row].state.w=0;return;
    }
    if(!bt_inside(position)) {
        btDirect[row]=float4(0,0,0,asfloat(btWork.z));
        btEnvironment[row]=float4(0,0,0,asfloat(btWork.z));return;
    }
    float3 direct=bt_local_area_direct(surface,batch,random);
    float cosine=saturate(dot(normal,btSun.xyz));
    float3 origin;bool originValid=bt_outward_origin(position,normal,origin);
    float sunDistance=originValid?bt_sky_distance(origin,btSun.xyz):0;
    if(cosine>0 && btSun.w>0 && sunDistance>0 && sunDistance<=raySettings.y) {
        InterlockedAdd(btCounters[7],1);
        float visible=world_trace_ray(origin,btSun.xyz,sunDistance,true,float3(1e30),false,true,true,0).hit?0:1;
        direct+=cosine*btSun.w*visible/3.14159265359;
    }
    float3 environment=0;
    for(uint slot=0;slot<btFrameInfo.w;++slot) {
        if(btLinks[row*btFrameInfo.w+slot].w!=2)continue;
        float3 origin,direction;if(!bt_surface_sample(surface,slot,batch,origin,direction))continue;
        environment+=diffuse_environment(direction,btSun.xyz,btSky)/float(btFrameInfo.w);
    }
    bool oldValid=surface.state.w>0 && (surface.extra.w&2)==0 && bt_epoch(btDirect[row].w,btWork.z);
    uint count=oldValid?min(surface.state.w+1,16u):1;
    float weight=1.0/float(count);
    float3 oldDirect=oldValid?btDirect[row].rgb:0;
    float3 oldEnvironment=oldValid && bt_epoch(btEnvironment[row].w,btWork.z)?btEnvironment[row].rgb:0;
    btDirect[row]=float4(lerp(oldDirect,direct,weight),asfloat(btWork.z));
    btEnvironment[row]=float4(lerp(oldEnvironment,environment,weight),asfloat(btWork.z));
    btSurfaces[row].state.w=count;
    InterlockedAnd(btSurfaces[row].extra.w,0xfffffffdu);
}
