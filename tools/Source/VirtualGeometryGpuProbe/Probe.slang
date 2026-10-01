[__AttributeUsage(_AttributeTargets.Var)]
struct rootAttribute {};
[root] RWStructuredBuffer<uint64_t> winners;
RWStructuredBuffer<uint> arguments;
RWStructuredBuffer<uint> counts;
RWStructuredBuffer<uint> executions;

[shader("compute")]
[numthreads(64,1,1)]
void prepare(uint lane:SV_GroupIndex) {
    uint64_t candidate=(uint64_t(lane+1)<<32)|uint64_t(63-lane);
    uint64_t previous;
    InterlockedMax(winners[0],candidate,previous);
    InterlockedMin(winners[1],candidate,previous);
    if(lane==0) {
        // Skip the first argument record and count word to exercise offsets.
        arguments[0]=99;arguments[1]=1;arguments[2]=1;
        arguments[3]=3;arguments[4]=1;arguments[5]=1;
        arguments[6]=5;arguments[7]=1;arguments[8]=1;
        arguments[9]=0;arguments[10]=1;arguments[11]=1;
        counts[0]=99;counts[1]=1;counts[2]=0;counts[3]=2;
    }
}
struct Output { float4 position:SV_Position; };
[shader("mesh")]
[outputtopology("triangle")]
[numthreads(32,1,1)]
void meshMain(uint lane:SV_GroupIndex,out vertices Output vertices[3],out indices uint3 triangles[1]) {
    SetMeshOutputCounts(3,1);
    if(lane==0) {
        uint previous;InterlockedAdd(executions[0],1,previous);
        vertices[0].position=float4(-.5,-.5,.5,1);
        vertices[1].position=float4(0,.5,.5,1);
        vertices[2].position=float4(.5,-.5,.5,1);
        triangles[0]=uint3(0,1,2);
    }
}
[shader("fragment")]
float4 fragmentMain(Output input):SV_Target {return float4(1,0,0,1);}
