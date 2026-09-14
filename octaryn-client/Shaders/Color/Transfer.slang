#pragma once
// Original present.frag.glsl transfer; inverse adapts its pre-present UI blend to an sRGB target.
float3 original_linear_to_srgb(float3 color) {
    color=max(color,float3(0));
    return select(color<=0.0031308,color*12.92,1.055*pow(color,1.0/2.4)-0.055);
}
float3 original_srgb_to_linear(float3 color) {
    color=max(color,float3(0));
    return select(color<=0.04045,color/12.92,pow((color+0.055)/1.055,2.4));
}
