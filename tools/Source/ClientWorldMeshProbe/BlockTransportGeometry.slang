#include "../../../octaryn-client/Shaders/Player/Geometry.slang"
#include "../../../octaryn-client/Shaders/WorldItems/Geometry.slang"
struct GeometryInput {float4 columns[4];float4 position,normal,feet,settings;};
struct GeometryOutput {float4 position,normal,geometricNormal;};
StructuredBuffer<GeometryInput> geometryInputs;
RWStructuredBuffer<GeometryOutput> geometryOutputs;
uniform uint geometryCount;
[shader("compute")]
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=geometryCount)return;
    GeometryInput input=geometryInputs[id.x];GeometryOutput output;
    float3 position,normal,geometric=0;
    if(input.settings.z<0) {
        player_skinned_geometry(input.columns[0].xyz,input.columns[1].xyz,input.columns[2].xyz,
            input.columns[3].xyz,input.position.xyz,input.normal.xyz,input.settings.xy,position,normal);
        position+=input.feet.xyz;
    } else {
        uint face=uint(input.settings.z);bool sprite=input.settings.w!=0;
        float phase=input.settings.y,time=input.settings.x;
        float3 a=item_offset(phase,item_face_point(face,float2(0,0),sprite),time);
        float3 b=item_offset(phase,item_face_point(face,float2(1,0),sprite),time);
        float3 c=item_offset(phase,item_face_point(face,float2(1,1),sprite),time);
        float3 d=item_offset(phase,item_face_point(face,float2(0,1),sprite),time);
        position=input.feet.xyz+(a+b+c+d)*.25;
        normal=item_rotate(item_face_normal(face,sprite),time*1.6+phase);
        geometric=normalize(cross(b-a,c-a));
    }
    output.position=float4(position,0);output.normal=float4(normal,0);
    output.geometricNormal=float4(geometric,0);geometryOutputs[id.x]=output;
}
