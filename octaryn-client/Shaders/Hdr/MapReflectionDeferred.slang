// Direct-map only. The owner rejects active transport GI and procedural scenes.
float3 map_deferred_normal(WorldRayHit hit,float3 direction) {
    MapVertex va=map_ray_vertex(hit.instance,hit.primitive*3);
    MapVertex vb=map_ray_vertex(hit.instance,hit.primitive*3+1);
    MapVertex vc=map_ray_vertex(hit.instance,hit.primitive*3+2);
    float3 a=map_vertex_position(va),b=map_vertex_position(vb),c=map_vertex_position(vc);
    float3 geometric=normalize(cross(b-a,c-a));
    bool backFace=dot(geometric,direction)>0;
    MapRayMaterial material=map_ray_material(hit.instance,hit.material);
    float3 weight=float3(1-hit.uv.x-hit.uv.y,hit.uv.x,hit.uv.y);
    float3 normal=map_vertex_normal(va)*weight.x+map_vertex_normal(vb)*weight.y+map_vertex_normal(vc)*weight.z;
    normal=dot(normal,normal)>1e-12?normalize(normal):geometric;
    float2 uv=va.uv*weight.x+vb.uv*weight.y+vc.uv*weight.z;
    float2 uv1=va.uv1*weight.x+vb.uv1*weight.y+vc.uv1*weight.z;
    float4 tangent=va.tangent*weight.x+vb.tangent*weight.y+vc.tangent*weight.z;
    if(material.textures[2].present!=0 &&
        (dot(tangent.xyz,tangent.xyz)<1e-10 || map_texture_frame_changed(material.textures[2]))) {
        float2 ta=map_texture_uv(material.textures[2],va.uv,va.uv1);
        float2 tb=map_texture_uv(material.textures[2],vb.uv,vb.uv1)-ta;
        float2 tc=map_texture_uv(material.textures[2],vc.uv,vc.uv1)-ta;
        float determinant=tb.x*tc.y-tb.y*tc.x;
        tangent.xyz=abs(determinant)>1e-10?((b-a)*tc.y-(c-a)*tb.y)/determinant:float3(0);
        float3 bitangent=abs(determinant)>1e-10?((c-a)*tb.x-(b-a)*tc.x)/determinant:float3(0);
        tangent.w=dot(cross(normal,tangent.xyz),bitangent)<0?-1:1;
    }
    // Only the normal result is consumed; the compiler removes other texture
    // evaluations while preserving the exact shared normal-map arithmetic.
    float3 mapped=map_surface(material,uv,uv1,float4(1),normal,tangent.xyz,tangent.w,
        float2(0),float2(0),float2(0),float2(0)).normal;
    return backFace?-mapped:mapped;
}
float3 map_deferred_material(WorldRayHit hit,float3 view,float4 sun,float visibility) {
    MapRayMaterial material=map_ray_material(hit.instance,hit.material);
    bool emission=any(material.emissive!=0);
    if(visibility==0) {
        ray_diagnostic(19);
        if(!emission)return 0;
        ray_diagnostic(20);
    }
    MapVertex va=map_ray_vertex(hit.instance,hit.primitive*3);
    MapVertex vb=map_ray_vertex(hit.instance,hit.primitive*3+1);
    MapVertex vc=map_ray_vertex(hit.instance,hit.primitive*3+2);
    float3 weight=float3(1-hit.uv.x-hit.uv.y,hit.uv.x,hit.uv.y);
    float2 uv=va.uv*weight.x+vb.uv*weight.y+vc.uv*weight.z;
    float2 uv1=va.uv1*weight.x+vb.uv1*weight.y+vc.uv1*weight.z;
    float3 radiance=emission?material.emissive*
        map_texture_sample(material.textures[4],uv,uv1,float2(0),float2(0),float2(0),float2(0)).rgb:float3(0);
    if(visibility==0)return radiance;
    float4 color=va.color*weight.x+vb.color*weight.y+vc.color*weight.z;
    float3 albedo=(material.baseColor*color*
        map_texture_sample(material.textures[0],uv,uv1,float2(0),float2(0),float2(0),float2(0))).rgb;
    float4 mr=map_texture_sample(material.textures[1],uv,uv1,float2(0),float2(0),float2(0),float2(0));
    float metallic=saturate(material.metallic*mr.b),roughness=saturate(material.roughness*mr.g);
    return radiance+pbr_direct(albedo,metallic,roughness,hit.normal,view,sun.xyz)*sun.w*visibility;
}
float3 map_reflected_radiance(WorldRayHit hit,float3 view,float4 sun,float4 sky,inout bool dynamic) {
    if(!hit.hit)return 0;
    ray_diagnostic(17);
    ray_diagnostic_max(12,mapDynamicInstanceStart);
    ray_diagnostic_max(13,hit.instance&0x7fffffu);
    if(world_ray_dynamic(hit))ray_diagnostic(8);
    dynamic=dynamic || world_ray_dynamic(hit);
    hit.normal=map_deferred_normal(hit,-view);
    float visibility=0;
    if(sun.w!=0 && dot(hit.normal,sun.xyz)>0) {
        float3 direction=normalize(sun.xyz);
        ray_diagnostic(7);
        WorldRayHit blocker=world_trace_ray(hit.position+hit.normal*.001+direction*.0005,direction,raySettings.y,true);
        if(world_ray_dynamic(blocker))ray_diagnostic(9);
        dynamic=dynamic || world_ray_dynamic(blocker);visibility=blocker.hit?0:1;
    }
    if(visibility==0)ray_diagnostic(18);
    return map_deferred_material(hit,view,sun,visibility);
}
