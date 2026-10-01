RWStructuredBuffer<uint> reflectionCounts,reflectionArguments;
uniform uint recoveryRayBudget;
#include "MapReflectionAdmission.slang"
[shader("compute")]
[numthreads(1,1,1)]
void clear_main(uint3 id:SV_DispatchThreadID) {
    for(uint i=0;i<8;++i)reflectionCounts[i]=0;
}
[shader("compute")]
[numthreads(1,1,1)]
void arguments_main(uint3 id:SV_DispatchThreadID) {
    for(uint i=0;i<3;++i) {
        uint2 admitted=reflection_recovery_admission(reflectionCounts[2],reflectionCounts[3],recoveryRayBudget);
        uint count=i<2?reflectionCounts[i]:admitted.x+admitted.y;
        uint groups=(count+63)/64;
        reflectionArguments[i*3]=min(groups,65535u);
        reflectionArguments[i*3+1]=max(1u,(groups+65534u)/65535u);reflectionArguments[i*3+2]=1;
    }
}
