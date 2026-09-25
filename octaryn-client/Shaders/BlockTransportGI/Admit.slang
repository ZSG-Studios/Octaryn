#include "Cache.slang"
#include "WorldAdmission.slang"
ByteAddressBuffer btWorldFaces;
uniform uint4 btAdmissionFaces; // first packed face, count, buffer face count, reserved
uniform int4 btAdmissionMin,btAdmissionMax; // inclusive minimum, exclusive maximum
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 group:SV_GroupID,uint3 lane:SV_GroupThreadID) {
    if(group.x>=btAdmissionFaces.y || btAdmissionFaces.x>=btAdmissionFaces.z ||
        group.x>=btAdmissionFaces.z-btAdmissionFaces.x)return;
    uint4 face=btWorldFaces.Load4((btAdmissionFaces.x+group.x)*16);
    uint count=bt_world_surface_count(face);
    for(uint ordinal=lane.x;ordinal<count;ordinal+=64) {
        int4 key;float4 reflectance;
        if(!bt_world_surface(face,ordinal,key,reflectance))continue;
        if(any(key.xyz<btAdmissionMin.xyz) || any(key.xyz>=btAdmissionMax.xyz))continue;
        // Resident world rows stay pinned for this geometry/residency epoch.
        bt_admit(key,reflectance,true,true);
    }
}
