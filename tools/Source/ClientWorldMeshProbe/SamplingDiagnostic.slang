#include "WorldRaster.slang"
float4 encode_bits(float value) {
    uint bits=asuint(value);return float4(bits&255,(bits>>8)&255,(bits>>16)&255,bits>>24);
}
float4 diagnostic_albedo(VertexOutput input,inout float2 uv,float2 dx,float2 dy) {
    float distance=length(input.relative);
    if(materialSettings.x!=0 && materialSettings.y!=0 && distance<kTerrainPomDistance &&
        terrain_face_edge_weight(atlasNormal,uv)>.001)
        uv=apply_terrain_parallax(atlasNormal,uv,dx,dy,-input.relative/max(distance,.001),distance,input.voxel,input.position.xy).uv;
    float4 albedo=is_sprite_voxel(input.voxel)?sample_sprite_albedo_grad(uv,input.voxel,dx,dy):
        sample_face_albedo_grad(uv,input.voxel,dx,dy);
    if((input.flags&1)==0 && albedo.a<.35) discard;
    return albedo;
}
[shader("fragment")]
GBufferOutput sampling_diagnostic(VertexOutput input) {
    GBufferOutput output;
    float2 uv=input.uv,dx=ddx(uv),dy=ddy(uv);
    diagnostic_albedo(input,uv,dx,dy);
    output.color=encode_bits(uv.x);output.relative=encode_bits(uv.y);
    // Probe-only specular binding has each mip filled with its integer level.
    output.voxel=encode_bits(atlasSpecular.SampleGrad(atlasCutout,float3(uv,get_index(input.voxel)),dx,dy).x)/255;
    output.material=encode_bits(max(length(dx*32),length(dy*32)))/255;
    return output;
}
[shader("fragment")]
GBufferOutput sampling_derivatives(VertexOutput input) {
    GBufferOutput output;
    float2 uv=input.uv,dx=ddx(uv),dy=ddy(uv);
    diagnostic_albedo(input,uv,dx,dy);
    output.color=encode_bits(dx.x);output.relative=encode_bits(dx.y);
    output.voxel=encode_bits(dy.x)/255;output.material=encode_bits(dy.y)/255;
    return output;
}
