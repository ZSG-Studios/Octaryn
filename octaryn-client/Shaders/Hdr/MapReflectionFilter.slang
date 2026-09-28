Texture2D<float4> radiance;
Texture2D<float> depths;
Texture2D<float4> surfaces;
Texture2D<float4> materials;
Texture2D<float2> moments;
RWTexture2D<float4> filtered;
uniform float4 dimensions;
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(any(id.xy>=uint2(dimensions.xy)))return;
    float4 center=radiance[id.xy],surface=surfaces[id.xy],material=materials[id.xy];
    float depth=depths[id.xy];
    // Never blur sharp reflections, silhouettes or invalid receivers.
    if(depth<=0 || surface.w<.15) {filtered[id.xy]=center;return;}
    float2 moment=moments[id.xy];
    float variance=max(0,moment.y-moment.x*moment.x);
    float instability=max(saturate(variance/max(.01,moment.x*moment.x)),1/max(center.a,1));
    float strength=smoothstep(.15,.55,surface.w)*saturate(instability*4);
    if(strength<.01) {filtered[id.xy]=center;return;}
    float3 sum=center.rgb*4;float total=4;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
        if(x==0 && y==0)continue;
        int2 tap=int2(id.xy)+int2(x,y);
        if(any(tap<0) || any(tap>=int2(dimensions.xy)))continue;
        float otherDepth=depths[tap];float4 otherSurface=surfaces[tap],otherMaterial=materials[tap];
        if(otherDepth<=0 || abs(otherDepth-depth)>max(.025,depth*.002) ||
            dot(surface.xyz,otherSurface.xyz)<.98 || abs(surface.w-otherSurface.w)>.03 ||
            distance(material.rgb,otherMaterial.rgb)>.035 || abs(material.a-otherMaterial.a)>.035)continue;
        float weight=(x==0 || y==0)?2:1;
        sum+=radiance[tap].rgb*weight;total+=weight;
    }
    filtered[id.xy]=float4(lerp(center.rgb,sum/total,strength),center.a);
}
