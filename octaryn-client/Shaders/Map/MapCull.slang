struct MapCullPrimitive {
    float4 minimum;float4 maximum;
    uint firstIndex;uint indexCount;uint material;uint opaque;
    uint4 lods;float4 errors;
};
StructuredBuffer<MapCullPrimitive> mapCullPrimitives;
RWStructuredBuffer<uint> mapDrawArguments;
RWStructuredBuffer<uint> mapCullFlags;
Texture2D<float> mapHizPyramid;
SamplerState mapHizSampler;
uniform float4 mapCullPlanes[6];
uniform float4 mapCullEye;
uniform uint4 mapCullSettings;
uniform float4 mapLodForward;
uniform float4 mapLodSettings;
uniform float4 mapCullTransform[4];
uniform float4 mapCullHiz;
// Two-phase occlusion culling (Wihlidal, "GPU-Driven Rendering Pipelines",
// Frostbite). Phase 0 culls against the previous frame's pyramid and records
// per-primitive results: 0=frustum culled, 1=drawn, 2=occluded. Phase 1
// retests the occluded set against the pyramid built from phase-0 depth and
// emits the newly visible. mapCullHiz=(width,height,mipCount,enabled).
bool map_occluded(float3 lo,float3 hi) {
    float3 right=mapCullTransform[0].xyz,up=mapCullTransform[1].xyz,forward=mapCullTransform[2].xyz;
    float2 jitter=float2(mapCullTransform[0].w,mapCullTransform[1].w);
    float2 minimum=float2(1e9),maximum=float2(-1e9);float nearest=1e9;
    [unroll]for(uint corner=0;corner<8;++corner) {
        float3 p=float3(corner&1?hi.x:lo.x,corner&2?hi.y:lo.y,corner&4?hi.z:lo.z);
        float depth=dot(p,forward);
        if(depth<=.1)return false;
        float2 ndc=float2(dot(p,right)*mapCullTransform[3].x,dot(p,up)*mapCullTransform[3].y)/depth;
        nearest=min(nearest,mapCullTransform[3].z-mapCullTransform[3].w/depth);
        float2 uv=(ndc+jitter)*float2(.5,-.5)+.5;
        minimum=min(minimum,uv);maximum=max(maximum,uv);
    }
    float2 rect=(clamp(maximum,0,1)-clamp(minimum,0,1))*mapCullHiz.xy+2;
    float mip=min(ceil(log2(max(max(rect.x,rect.y),1))),mapCullHiz.z-1);
    float2 center=(clamp(minimum,0,1)+clamp(maximum,0,1))*.5;
    return nearest>mapHizPyramid.SampleLevel(mapHizSampler,center,mip)+1e-6;
}
[shader("compute")]
[numthreads(64,1,1)]
void cull_main(uint3 dispatch:SV_DispatchThreadID) {
    uint index=dispatch.x;
    if(index>=mapCullSettings.x)return;
    MapCullPrimitive primitive=mapCullPrimitives[index];
    bool visible=primitive.opaque!=0;
    float3 lo=primitive.minimum.xyz-mapCullEye.xyz,hi=primitive.maximum.xyz-mapCullEye.xyz;
    if(mapCullSettings.y!=0)for(uint i=0;i<6;++i) {
        float4 plane=mapCullPlanes[i];
        float3 point=select(plane.xyz>=0,hi,lo);
        if(dot(plane.xyz,point)+plane.w<0)visible=false;
    }
    if(mapCullSettings.z==0) {
        bool occluded=visible && mapCullHiz.w!=0 && map_occluded(lo,hi);
        mapCullFlags[index]=!visible?0u:occluded?2u:1u;
        visible=visible && !occluded;
    } else if(mapCullFlags[index]!=2u || !visible)visible=false;
    else visible=mapCullHiz.w!=0 && !map_occluded(lo,hi);
    uint first=primitive.firstIndex,count=primitive.indexCount;
    float nearest=dot((lo+hi)*.5,mapLodForward.xyz)-length((hi-lo)*.5);
    if(visible && mapLodSettings.y>0 && nearest>.01)for(uint level=0;level<2;++level) {
        uint lodCount=primitive.lods[level*2+1];
        float pixels=primitive.errors[level]*mapLodSettings.x/nearest;
        if(lodCount!=0 && lodCount<count && pixels<=mapLodSettings.y) {
            first=primitive.lods[level*2];count=lodCount;
        }
    }
    // D3D12_DRAW_INDEXED_ARGUMENTS and VkDrawIndexedIndirectCommand share this ABI.
    uint offset=index*5;
    mapDrawArguments[offset]=count;
    mapDrawArguments[offset+1]=visible?1:0;
    mapDrawArguments[offset+2]=first;
    mapDrawArguments[offset+3]=0;
    mapDrawArguments[offset+4]=primitive.material;
}
