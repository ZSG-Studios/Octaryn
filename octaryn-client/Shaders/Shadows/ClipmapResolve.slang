#include "../Lighting/Surface.slang"
Texture2D<float> level0;
Texture2D<float> level1;
Texture2D<float> level2;
Texture2D<float4> positions;
Texture2D<float4> voxels;
RWTexture2D<float> visibility;
uniform float4 eye;
uniform float4 centers[3];
uniform float4 lightRight;
uniform float4 lightUp;
uniform float4 lightForward;
uniform uint2 extent;
float depth_at(uint level,int2 pixel) {
    if(level==0)return level0[pixel];if(level==1)return level1[pixel];return level2[pixel];
}
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=extent))return;
    float4 surface=positions[id.xy];float result=1;
    if(surface.w>0) {
        uint voxel=lighting_voxel(voxels[id.xy]);float3 normal=lighting_normal(voxel);
        if((voxel&256)!=0 && dot(normal,lightForward.xyz)>0)normal=-normal;
        float3 world=surface.xyz+eye.xyz+normal*.025;
        uint width,height;level0.GetDimensions(width,height);
        for(uint level=0;level<3;++level) {
            float3 p=world-centers[level].xyz;
            float2 clip=float2(dot(p,lightRight.xyz),dot(p,lightUp.xyz))/centers[level].w;
            float depth=dot(p,lightForward.xyz)/2048+.5;
            if(any(abs(clip)>.98) || depth<=0 || depth>=1)continue;
            int2 pixel=int2((clip*float2(.5,-.5)+.5)*float2(width,height));
            result=0;
            float forwardNormal=dot(normal,lightForward.xyz);
            float2 plane=float2(dot(normal,lightRight.xyz),dot(normal,lightUp.xyz));
            for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
                int2 tap=clamp(pixel+int2(x,y),int2(0),int2(width,height)-1);
                float2 tapClip=(float2(tap)+.5)/float2(width,height)*float2(2,-2)+float2(-1,1);
                // Compare each tap against the same geometric receiver plane.
                float correction=abs(forwardNormal)>1e-4?
                    dot(plane,(tapClip-clip)*centers[level].w)/(forwardNormal*2048):0;
                result+=depth-correction-.000004<=depth_at(level,tap)?1.0/9:0;
            }
            break;
        }
    }
    visibility[id.xy]=result;
}
