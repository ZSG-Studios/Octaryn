// Retained glTF geometry, authored joints/weights and metallic-roughness factors.
struct PlayerVertex {
    float4 position;
    float4 normal;
    float4 uv;
    uint4 joints;
    float4 weights;
};
StructuredBuffer<PlayerVertex> vertices;
StructuredBuffer<uint> indices;
uniform float4 jointColumns[256];
uniform float4 cameraPosition;
uniform float4 cameraRight;
uniform float4 cameraUp;
uniform float4 cameraForward;
uniform float4 projection;
uniform float4 feetPosition;
uniform float4 bodyRotation;
uniform float4 lightDirection;
uniform float4 lightingSettings;
uniform float4 baseColor;
uniform float4 material; // metallic, roughness, alpha cutoff, first index
uniform float4 previousJointColumns[256];
uniform float4 previousCameraPosition,previousCameraRight,previousCameraUp,previousCameraForward,previousProjection;
uniform float4 previousFeetPosition,previousBodyRotation;
#include "../BlockTransportGI/DynamicSample.slang"
#include "Geometry.slang"
struct VertexOutput {
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float3 relative : TEXCOORD1;
    float4 currentClip : TEXCOORD2;
    float4 previousClip : TEXCOORD3;
    float3 indirectFront : TEXCOORD4;
    float3 indirectBack : TEXCOORD5;
    float4 directFront : TEXCOORD6;
    float4 directBack : TEXCOORD7;
};
[shader("vertex")]
VertexOutput vertex_main(uint id : SV_VertexID) {
    uint vertexIndex=indices[uint(material.w)+id];
    PlayerVertex vertex=vertices[vertexIndex];
    float4 c0=0,c1=0,c2=0,c3=0;
    float4 previousSkinned=0;
    for(uint k=0;k<4;++k) {
        uint joint=vertex.joints[k]*4;float weight=vertex.weights[k];
        c0+=jointColumns[joint]*weight;c1+=jointColumns[joint+1]*weight;
        c2+=jointColumns[joint+2]*weight;c3+=jointColumns[joint+3]*weight;
        previousSkinned+=(previousJointColumns[joint]*vertex.position.x+previousJointColumns[joint+1]*vertex.position.y+
            previousJointColumns[joint+2]*vertex.position.z+previousJointColumns[joint+3])*weight;
    }
    float3 offset,normal;
    player_skinned_geometry(c0.xyz,c1.xyz,c2.xyz,c3.xyz,vertex.position.xyz,vertex.normal.xyz,
        bodyRotation.xy,offset,normal);
    VertexOutput output;
    output.indirectFront=bt_dynamic_incident(vertexIndex*2);
    output.indirectBack=bt_dynamic_incident(vertexIndex*2+1);
    output.directFront=bt_dynamic_direct(vertexIndex*2);
    output.directBack=bt_dynamic_direct(vertexIndex*2+1);
    output.normal=normal;
    output.relative=(feetPosition.xyz-cameraPosition.xyz)+offset;
    float depth=dot(output.relative,cameraForward.xyz);
    output.position=float4(dot(output.relative,cameraRight.xyz)*projection.x,
        dot(output.relative,cameraUp.xyz)*projection.y,depth*projection.z-projection.w,depth);
    output.currentClip=output.position;
    output.position.xy+=float2(cameraRight.w,cameraUp.w)*depth;
    float3 previousRelative=(previousFeetPosition.xyz-previousCameraPosition.xyz)+
        float3(previousBodyRotation.x*previousSkinned.x-previousBodyRotation.y*previousSkinned.z,
        previousSkinned.y,previousBodyRotation.y*previousSkinned.x+previousBodyRotation.x*previousSkinned.z);
    float previousDepth=dot(previousRelative,previousCameraForward.xyz);
    output.previousClip=float4(dot(previousRelative,previousCameraRight.xyz)*previousProjection.x,
        dot(previousRelative,previousCameraUp.xyz)*previousProjection.y,0,previousDepth);
    return output;
}
float4 shade_player(VertexOutput input,bool front) {
    if(baseColor.a<material.z) discard;
    float3 n=normalize(input.normal)*(front?1:-1);
    float3 v=normalize(-input.relative),l=normalize(lightDirection.xyz),h=normalize(v+l);
    float nv=max(dot(n,v),0.001),nl=saturate(dot(n,l)),nh=saturate(dot(n,h)),vh=saturate(dot(v,h));
    float roughness=clamp(material.y,0.04,1),a=roughness*roughness,a2=a*a;
    float d=a2/max(3.14159265*pow(nh*nh*(a2-1)+1,2),0.0001);
    float k=pow(roughness+1,2)/8;
    float g=nv/(nv*(1-k)+k)*nl/max(nl*(1-k)+k,0.0001);
    float3 f0=lerp(float3(0.04),baseColor.rgb,saturate(material.x));
    float3 f=f0+(1-f0)*pow(1-vh,5);
    float3 diffuse=(1-f)*(1-saturate(material.x))*baseColor.rgb/3.14159265;
    float3 specular=d*g*f/max(4*nv*nl,0.0001);
    float4 direct=front?input.directFront:input.directBack;
    float3 hdr=(diffuse+specular)*nl*lightingSettings.y*direct.w;
    hdr+=baseColor.rgb*(.96*(1-saturate(material.x)))*
        ((front?input.indirectFront:input.indirectBack)+direct.rgb);
    return float4(max(hdr,0),baseColor.a);
}
[shader("fragment")]
float4 fragment_main(VertexOutput input,bool front:SV_IsFrontFace):SV_Target0 {return shade_player(input,front);}
struct TemporalOutput {float4 color:SV_Target0;float4 motion:SV_Target1;};
[shader("fragment")]
TemporalOutput temporal_fragment_main(VertexOutput input,bool front:SV_IsFrontFace) {
    TemporalOutput output;output.color=shade_player(input,front);
    float2 previous=input.previousClip.xy/max(input.previousClip.w,1e-6);
    float2 current=input.currentClip.xy/max(input.currentClip.w,1e-6);
    output.motion=float4((previous-current)*float2(.5,-.5),1,0);return output;
}
