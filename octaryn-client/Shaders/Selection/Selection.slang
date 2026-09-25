uniform float4 cameraPosition;
uniform float4 cameraRight;
uniform float4 cameraUp;
uniform float4 cameraForward;
uniform float4 projection;
uniform float4 blockFace;
struct Varying {float4 position:SV_Position;};
[shader("vertex")]
Varying vertex_main(uint vertex:SV_VertexID) {
    const float2 corners[6]={float2(0,0),float2(1,0),float2(1,1),float2(0,0),float2(1,1),float2(0,1)};
    float2 c=corners[vertex];float3 p=0;uint face=uint(blockFace.w);
    if(face==0) p=float3(c.x,c.y,1);
    if(face==1) p=float3(1-c.x,c.y,0);
    if(face==2) p=float3(1,c.y,1-c.x);
    if(face==3) p=float3(0,c.y,c.x);
    if(face==4) p=float3(c.x,1,1-c.y);
    if(face==5) p=float3(c.x,0,c.y);
    p=(p-.5)*1.001+.5+blockFace.xyz-cameraPosition.xyz;
    float z=dot(p,cameraForward.xyz);
    Varying output;
    output.position=float4(dot(p,cameraRight.xyz)*projection.x,dot(p,cameraUp.xyz)*projection.y,z*projection.z-projection.w,z);
    output.position.xy+=float2(cameraRight.w,cameraUp.w)*z;
    return output;
}
[shader("fragment")]
float4 fragment_main(Varying v):SV_Target {return float4(1,1,1,.15);}
