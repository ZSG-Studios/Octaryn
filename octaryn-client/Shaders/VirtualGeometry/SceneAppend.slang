struct SceneDraw {
    uint cluster,transform,pageBase,materialBase;
    uint generation,padding0,padding1,padding2;
};
StructuredBuffer<uint4> sceneLocalSelected;
StructuredBuffer<uint> sceneLocalCounters;
RWStructuredBuffer<SceneDraw> sceneDraws;
RWStructuredBuffer<uint4> sceneSelected;
RWStructuredBuffer<uint> sceneCounters;
RWStructuredBuffer<uint> sceneDispatch;
uniform uint4 sceneDomain[2]; // bases; instance/cluster/page counts and material base
uniform uint2 sceneLimits; // draw capacity, frame generation
struct SceneDomain {uint4 bases,counts;uint rootCut,pad0,pad1,pad2;};
struct SceneCluster {
    uint group,refinedGroup,material,page;
    uint vertexOffset,vertexCount,triangleOffset,triangleCount,flags;
    float3 center;float radius,error;
};
StructuredBuffer<SceneDomain> sceneDomains;
StructuredBuffer<SceneCluster> sceneClusters;
StructuredBuffer<uint4> scenePages;
uniform uint sceneDomainCount;

[shader("compute")]
[numthreads(64,1,1)]
void roots_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    uint identity=(group.x+group.y*65535)*64+lane;
    uint lower=0,upper=sceneDomainCount;
    while(lower<upper) {
        uint middle=lower+(upper-lower)/2;
        if(sceneDomains[middle].bases.w<=identity)lower=middle+1;else upper=middle;
    }
    if(lower==0)return;SceneDomain domain=sceneDomains[lower-1];
    uint local=identity-domain.bases.w;
    if(domain.rootCut==0 || local>=domain.counts.x*domain.counts.y)return;
    uint cluster=local/domain.counts.x,instance=local%domain.counts.x;
    SceneCluster source=sceneClusters[domain.bases.x+cluster];
    if(source.page>=domain.counts.z || source.refinedGroup!=0xffffffff || identity>=sceneLimits.x) {
        InterlockedOr(sceneCounters[1],8);return;
    }
    uint pageIndex=domain.bases.y+source.page;uint4 page=scenePages[pageIndex];
    if(page.z==0 || page.y==0 || page.x==0xffffffff) {InterlockedOr(sceneCounters[1],16);return;}
    SceneDraw draw;
    draw.cluster=domain.bases.x+cluster;draw.transform=domain.bases.z+instance;
    draw.pageBase=domain.bases.y;draw.materialBase=domain.counts.w;
    draw.generation=sceneLimits.y;draw.padding0=draw.padding1=draw.padding2=0;sceneDraws[identity]=draw;
    uint slot;InterlockedAdd(sceneCounters[0],1,slot);
    if(slot>=sceneLimits.x) {InterlockedOr(sceneCounters[1],4);return;}
    sceneSelected[slot]=uint4(identity,pageIndex,page.x,page.y);
}

[shader("compute")]
[numthreads(64,1,1)]
void append_main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    uint item=(group.x+group.y*65535)*64+lane;
    uint instances=sceneDomain[1].x,clusters=sceneDomain[1].y,pages=sceneDomain[1].z;
    uint count=sceneLocalCounters[0];
    if(count>clusters || sceneLocalCounters[3]!=0 || sceneLocalCounters[4]!=0) {
        if(item==0)InterlockedOr(sceneCounters[1],1);return;
    }
    if(item>=count*instances)return;
    uint4 selected=sceneLocalSelected[item/instances];uint instance=item%instances;
    uint identity=sceneDomain[0].w+selected.x*instances+instance;
    if(selected.x>=clusters || selected.y>=pages || selected.w==0 || identity>=sceneLimits.x) {
        InterlockedOr(sceneCounters[1],2);return;
    }
    SceneDraw draw;
    draw.cluster=sceneDomain[0].x+selected.x;draw.transform=sceneDomain[0].z+instance;
    draw.pageBase=sceneDomain[0].y;draw.materialBase=sceneDomain[1].w;
    draw.generation=sceneLimits.y;draw.padding0=draw.padding1=draw.padding2=0;
    sceneDraws[identity]=draw;
    uint slot;InterlockedAdd(sceneCounters[0],1,slot);
    if(slot>=sceneLimits.x) {InterlockedOr(sceneCounters[1],4);return;}
    sceneSelected[slot]=uint4(identity,sceneDomain[0].y+selected.y,selected.z,selected.w);
}
[shader("compute")]
[numthreads(1,1,1)]
void finish_main() {
    uint count=sceneCounters[1]==0?min(sceneCounters[0],sceneLimits.x):0;
    if(sceneCounters[1]!=0)sceneCounters[0]=0;
    uint groups=(count+31)/32;
    sceneDispatch[0]=min(groups,65535);sceneDispatch[1]=max(1,(groups+65534)/65535);sceneDispatch[2]=1;
    sceneDispatch[3]=min(count,65535);sceneDispatch[4]=max(1,(count+65534)/65535);sceneDispatch[5]=1;
}
