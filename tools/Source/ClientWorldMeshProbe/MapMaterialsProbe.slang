#include "../../../octaryn-client/Shaders/RayTracing/WorldRayQuery.slang"
#include "../../../octaryn-client/Shaders/Lighting/Surface.slang"
RWStructuredBuffer<float4> mapProbeResults;
[shader("compute")]
[numthreads(1,1,1)]
void mip_main(uint3 id:SV_DispatchThreadID) {
    MapRayMaterial material=mapRayPrimitives[0],mask=mapRayPrimitives[1];
    mapProbeResults[0]=material.textures[0].image.SampleLevel(material.textures[0].sampler,float2(.5),1);
    mapProbeResults[1]=material.textures[2].image.SampleLevel(material.textures[2].sampler,float2(.5),1)*2-1;
    mapProbeResults[2]=material.textures[1].image.SampleLevel(material.textures[1].sampler,float2(.5),1);
    float4 alpha;
    for(uint i=0;i<4;++i)
        alpha[i]=mask.textures[0].image.SampleLevel(mask.textures[0].sampler,
            float2((i%2)*.5+.25,(i/2)*.5+.25),1).a;
    mapProbeResults[3]=alpha;
}
[shader("compute")]
[numthreads(1,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    MapRayMaterial material=mapRayPrimitives[1];
    MapVertex vertex=mapVertices[3];
    MapSurface surface=map_surface(material,vertex.uv,vertex.uv1,vertex.color,
        map_vertex_normal(vertex),vertex.tangent.xyz,vertex.tangent.w,0,0,0,0);
    mapProbeResults[0]=surface.albedo;
    mapProbeResults[1]=float4(surface.metallic,surface.roughness,surface.occlusion,0);
    mapProbeResults[2]=float4(surface.normal,0);
    mapProbeResults[3]=float4(surface.emission,0);
    mapProbeResults[4]=float4(lighting_normal(lighting_pack_map(surface.normal,surface.occlusion)),0);
    WorldRayHit hit=world_trace_ray(float3(.25,.25,2),float3(0,0,-1),10);
    WorldRayHit visibility=world_trace_ray(float3(.25,.25,2),float3(0,0,-1),10,true);
    mapProbeResults[1].w=visibility.hit?visibility.distance:-1;
    mapProbeResults[5]=float4(hit.hit?1:0,hit.distance,hit.material,hit.roughness);
    mapProbeResults[6]=hit.albedo;
    mapProbeResults[7]=float4(hit.emission,hit.metallic);
}
