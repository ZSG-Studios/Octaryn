#pragma once
uniform float4 CameraRight;
uniform float4 CameraUp;
uniform float4 CameraForward;
uniform float4 Projection;
struct SkyVertex { float4 position : SV_Position; float3 direction : TEXCOORD0; };
[shader("vertex")]
SkyVertex vertex_main(uint vertex : SV_VertexID) {
    const float2 corners[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
    float2 clip=corners[vertex];
    SkyVertex result;
    result.position=float4(clip,1,1);
    result.direction=CameraForward.xyz+CameraRight.xyz*((clip.x-CameraRight.w)/Projection.x)+CameraUp.xyz*((clip.y-CameraUp.w)/Projection.y);
    return result;
}
