// Motion is previous UV minus current UV, excluding camera projection jitter.
Texture2D<float> depth;
Texture2D<float4> objectMotion;
Texture2D<float4> opaqueColor;
RWTexture2D<float4> sceneColor;
Texture2D<float4> material;
RWTexture2D<float2> motion;
RWTexture2D<float> reactive;
uniform float4 currentPosition,currentRight,currentUp,currentForward,currentProjection;
uniform float4 previousPosition,previousRight,previousUp,previousForward,previousProjection;
uniform float4 dimensions; // width, height, jitter NDC X, jitter NDC Y
uniform float4 options; // reset
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
  if(any(id.xy>=uint2(dimensions.xy)))return;
  float2 uv=(float2(id.xy)+.5)/dimensions.xy;
  float2 ndc=float2(uv.x*2-1,1-uv.y*2)-dimensions.zw;
  float d=depth[id.xy];
  float distance=currentProjection.w/max(currentProjection.z-d,1e-8);
  float3 ray=currentForward.xyz+currentRight.xyz*(ndc.x/currentProjection.x)+currentUp.xyz*(ndc.y/currentProjection.y);
  float3 relative=ray*distance+(currentPosition.xyz-previousPosition.xyz);
  // Sky is at infinity: camera translation must not give the sky parallax.
  if(d>=1)relative=ray;
  float previousZ=dot(relative,previousForward.xyz);
  float2 previousNdc=float2(dot(relative,previousRight.xyz)*previousProjection.x,
      dot(relative,previousUp.xyz)*previousProjection.y)/max(previousZ,1e-6);
  float2 previousUv=float2(previousNdc.x*.5+.5,.5-previousNdc.y*.5);
  float2 currentUv=float2(ndc.x*.5+.5,.5-ndc.y*.5);
  float4 object=objectMotion[id.xy];
  float2 velocity=object.z>0?object.xy:previousUv-currentUv;
  velocity=clamp(velocity,-1,1);
  // Reserve Godot's (-1,-1) sentinel, including RG16F rounding near that corner.
  if(all(velocity<=-0.999755859375))velocity=float2(1,1);
  motion[id.xy]=options.x>0?float2(0):velocity;
  float3 before=opaqueColor[id.xy].rgb,after=sceneColor[id.xy].rgb;
  float difference=max(max(abs(before.x-after.x),abs(before.y-after.y)),abs(before.z-after.z));
  float magnitude=max(max(max(before.x,after.x),max(before.y,after.y)),max(before.z,after.z));
  // Forward layers and emissive animated materials must not leave stale history.
  // Opaque objects cover the terrain material stored in the earlier G-buffer.
  float emission=object.z>0?0:saturate(material[id.xy].a);
  reactive[id.xy]=min(.9,max(saturate(difference/max(.01,magnitude)),emission));
  // The original sky is authored in display-linear color and inverse tone
  // mapped for composition. Reconstruct in that original bounded domain;
  // otherwise a saturated blue sky becomes 999 radiance at silhouette taps.
  float3 color=max(after,0);
  sceneColor[id.xy]=float4(color/(color+1),1);
}
