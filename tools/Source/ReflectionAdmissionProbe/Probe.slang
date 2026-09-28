#include "../../../octaryn-client/Shaders/Hdr/MapReflectionAdmission.slang"
StructuredBuffer<uint3> cases;
RWStructuredBuffer<uint2> results;
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint3 c=cases[id.x];results[id.x]=reflection_recovery_admission(c.x,c.y,c.z);
}
