// Original shaders/present.frag.glsl tone mapping; UI is composed afterward.
#include "../Materials/PbrEvaluate.slang"
Texture2D<float4> scene;
RWTexture2D<float4> output;
uniform uint4 options; // x: reconstructed input is already tone-mapped linear.
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
  uint width,height;output.GetDimensions(width,height);if(id.x>=width || id.y>=height) return;
  float3 color=scene[id.xy].rgb;
  if(options.x!=0) {
    color=max(color,0);
    color=select(color<=0.0031308,color*12.92,1.055*pow(color,1.0/2.4)-0.055);
  } else color=original_present(color);
  output[id.xy]=float4(color,1);
}
