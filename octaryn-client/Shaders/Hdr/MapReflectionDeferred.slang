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
    if(!map_material_zero_basis(material) && material.textures[2].present!=0 &&
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
    float3 mapped=map_surface(material,uv,uv1,float4(1),normal,tangent.xyz,tangent.w,va.blend0*weight.x+vb.blend0*weight.y+vc.blend0*weight.z,
        va.blend1*weight.x+vb.blend1*weight.y+vc.blend1*weight.z,
        float2(0),float2(0),float2(0),float2(0)).normal;
    return backFace?-mapped:mapped;
}
// Reflected hits carry no screen gradients; distance minification dominates,
// so sample an explicit conservative level without widening the lobe.
float map_reflection_mip(float distance) {return clamp(log2(max(1,distance)),0,10);}
float4 map_texture_level(MapTexture t,float2 uv,float2 uv1,float mip) {
    if(t.present==0)return float4(1);
    return nonuniform(t.image).SampleLevel(nonuniform(t.sampler),map_texture_uv(t,uv,uv1),mip);
}
float3 map_deferred_material(WorldRayHit hit,float3 view,float4 sun,float4 sky,float visibility) {
    MapRayMaterial material=map_ray_material(hit.instance,hit.material);
    bool emission=any(material.emissive!=0);
    if(visibility==0) {
        ray_diagnostic(19);
        if(emission)ray_diagnostic(20);
    }
    MapVertex va=map_ray_vertex(hit.instance,hit.primitive*3);
    MapVertex vb=map_ray_vertex(hit.instance,hit.primitive*3+1);
    MapVertex vc=map_ray_vertex(hit.instance,hit.primitive*3+2);
    float3 weight=float3(1-hit.uv.x-hit.uv.y,hit.uv.x,hit.uv.y);
    float2 uv=va.uv*weight.x+vb.uv*weight.y+vc.uv*weight.z;
    float2 uv1=va.uv1*weight.x+vb.uv1*weight.y+vc.uv1*weight.z;
    float mip=map_reflection_mip(hit.distance);
    float3 radiance=emission?material.emissive*
        map_texture_level(material.textures[4],uv,uv1,mip).rgb:float3(0);
    float4 color=va.color*weight.x+vb.color*weight.y+vc.color*weight.z;
    float4 blend0=va.blend0*weight.x+vb.blend0*weight.y+vc.blend0*weight.z;
    float4 blend1=va.blend1*weight.x+vb.blend1*weight.y+vc.blend1*weight.z;
    uint layers=material.padding.y>>8;
    float4 diffuse=layers==0?map_texture_level(material.textures[0],uv,uv1,mip):float4(0);
    for(uint i=0;i<layers;++i)diffuse+=(i<4?blend0[i]:blend1[i-4])*map_texture_level(material.textures[5+i*2],uv,uv1,mip);
    float3 albedo=(material.baseColor*color*diffuse).rgb;
    if(map_material_unlit(material))return albedo;
    float4 mr=map_texture_level(material.textures[1],uv,uv1,mip);
    float metallic=saturate(material.metallic*mr.b),roughness=saturate(material.roughness*mr.g);
    float occlusion=lerp(1,map_texture_level(material.textures[3],uv,uv1,mip).r,material.occlusionStrength);
    // Shadowed and night reflected hits still receive the sky ambient the
    // primary surface gets in Composite. skylight_floor is not bound to this
    // pass; a zero floor only darkens downfacing hits, never brightens them.
    float3 fresnel=pbr_environment_reflectance(lerp(float3(.04),albedo,metallic),roughness,dot(hit.normal,view));
    float3 ambient=scene_diffuse_ambient(hit.normal,sun.xyz,sky,
        original_ambient_visibility(hit.normal,false,0,max(sky.x,.42)))*occlusion;
    float3 direct;
    if(map_material_zero_basis(material)) {
        float3 n=map_texture_level(material.textures[2],uv,uv1,mip).rgb*2-1;n.xy*=material.normalScale;
        direct=pbr_direct_zero_plane(albedo,metallic,roughness,hit.normal,view,sun.xyz,n.z/max(length(n),1e-20));
    } else direct=pbr_direct(albedo,metallic,roughness,hit.normal,view,sun.xyz);
    return radiance+albedo*(1-metallic)*(1-fresnel)*ambient+direct*sun.w*visibility*scene_direct_color();
}
float3 map_reflected_radiance(WorldRayHit hit,float3 view,float4 sun,float4 sky,inout bool dynamic) {
    if(!hit.hit)return 0;
    ray_diagnostic(17);
    ray_diagnostic_max(12,mapDynamicInstanceStart);
    ray_diagnostic_max(13,hit.instance&0x7fffffu);
    if(world_ray_dynamic(hit))ray_diagnostic(8);
    dynamic=dynamic || world_ray_dynamic(hit);
    if(map_material_unlit(map_ray_material(hit.instance,hit.material)))
        return map_deferred_material(hit,view,sun,sky,1);
    hit.normal=map_deferred_normal(hit,-view);
    float visibility=0;
    if(sun.w!=0 && (map_material_zero_basis(map_ray_material(hit.instance,hit.material)) || dot(hit.normal,sun.xyz)>0)) {
        float3 direction=normalize(sun.xyz);
        ray_diagnostic(7);
        WorldRayHit blocker=world_trace_ray(hit.position+hit.normal*.001+direction*.0005,direction,raySettings.y,true);
        if(world_ray_dynamic(blocker))ray_diagnostic(9);
        // Same receiver-blocker fade-to-lit constants as Shadow.slang's
        // zero-shadow-range path; nearby blockers stay dark, far ones soften.
        dynamic=dynamic || world_ray_dynamic(blocker);
        visibility=blocker.hit?saturate((blocker.distance/raySettings.y-.875)*8):1;
    }
    if(visibility==0)ray_diagnostic(18);
    return map_deferred_material(hit,view,sun,sky,visibility);
}
