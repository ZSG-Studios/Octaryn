#ifndef OCTARYN_LOCAL_SHADOW_SAMPLE
#define OCTARYN_LOCAL_SHADOW_SAMPLE
Texture2D<float> localShadow0,localShadow1,localShadow2,localShadow3,localShadow4,localShadow5;
uniform uint4 localShadowInfo; // selected light ID, valid, resolution, unused
uniform float4 localShadowPosition,localShadowProjection; // A, B, world origin bias, near plane
float local_shadow_depth(uint face,int2 pixel) {
    if(face==0)return localShadow0[pixel];if(face==1)return localShadow1[pixel];
    if(face==2)return localShadow2[pixel];if(face==3)return localShadow3[pixel];
    if(face==4)return localShadow4[pixel];return localShadow5[pixel];
}
float local_shadow_visibility(uint light,float3 position,float3 normal) {
    if(localShadowInfo.y==0 || light!=localShadowInfo.x)return 1;
    float3 fromLight=position-localShadowPosition.xyz;
    if(length(fromLight)>localShadowPosition.w)return 1;
    float sign=dot(normal,fromLight)>0?-1:1;
    fromLight+=normal*sign*localShadowProjection.z;
    float3 a=abs(fromLight);uint face;
    if(a.x>=a.y && a.x>=a.z)face=fromLight.x>=0?0:1;
    else if(a.y>=a.z)face=fromLight.y>=0?2:3;
    else face=fromLight.z>=0?4:5;
    const float3 right[6]={float3(0,0,1),float3(0,0,-1),float3(1,0,0),float3(1,0,0),float3(-1,0,0),float3(1,0,0)};
    const float3 up[6]={float3(0,1,0),float3(0,1,0),float3(0,0,1),float3(0,0,-1),float3(0,1,0),float3(0,1,0)};
    const float3 forward[6]={float3(1,0,0),float3(-1,0,0),float3(0,1,0),float3(0,-1,0),float3(0,0,1),float3(0,0,-1)};
    float z=dot(fromLight,forward[face]);
    if(z<=localShadowProjection.w)return 1;
    float2 clip=float2(dot(fromLight,right[face]),dot(fromLight,up[face]))/z;
    int2 pixel=int2((clip*float2(.5,-.5)+.5)*localShadowInfo.z);
    float3 localNormal=float3(dot(normal,right[face]),dot(normal,up[face]),dot(normal,forward[face]));
    float plane=dot(normal,fromLight);
    float result=0;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
        int2 tap=clamp(pixel+int2(x,y),int2(0),int2(localShadowInfo.zz)-1);
        float2 tapClip=(float2(tap)+.5)/float(localShadowInfo.z)*float2(2,-2)+float2(-1,1);
        float denominator=dot(localNormal,float3(tapClip,1));
        float receiverZ=abs(denominator)>1e-6?plane/denominator:z;
        float depth=localShadowProjection.x-localShadowProjection.y/max(receiverZ,localShadowProjection.w);
        result+=depth-.000004<=local_shadow_depth(face,tap)?1.0/9:0;
    }
    return result;
}
#endif
