#include "BlockTransportWorldAdmission.h"

namespace mesh_probe {
void block_transport_admission_cases(Fixture& f) {
  using namespace world_admission;auto& r=f.renderer;
  require(prepare_world_atlas_plant_masks(r.atlas),"BT resident admission plant masks");
  auto source=column(-2,3,-32,32);const auto stone=material(f,"stone"),leaves=material(f,"leaves");
  for(int z=3;z<12;++z)for(int y=5;y<13;++y)for(int x=4;x<15;++x)
    put(source,x,y,z,static_cast<std::uint16_t>(stone));
  put(source,20,8,20,static_cast<std::uint16_t>(leaves));
  unsigned x=4;
  for(const char* plant:{"rose","gardenia","bluebell","lavender","bush"}) {
    put(source,int(x),20,20,static_cast<std::uint16_t>(material(f,plant)));x+=4;
  }
  put(source,24,8,24,static_cast<std::uint16_t>(material(f,"glass")));
  put(source,26,8,26,static_cast<std::uint16_t>(material(f,"red_torch")));
  put(source,24,10,24,static_cast<std::uint16_t>(material(f,"water")));
  const auto mesh=f.mesh(source);f.verify("block_transport_world_admission",source,mesh);
  expansion(f,mesh);
  r.columns.clear();world_renderer_store_column(r,{-2,3},mesh.gpu);
  const Key low{-80,-52,58,0},high{-16,12,122,0};const auto wanted=expected(f,source,low,high);
  require(wanted.size()>300,"BT world admission fixture lacks merged world surfaces");
  const auto width=r.width,height=r.height;const bool culling=r.culling_enabled;r.culling_enabled=true;
  struct View {int width,height;float yaw;bool offscreen;};
  const View views[]{{320,180,3.14159265359f,false},{1280,720,0,true},{47,31,3.14159265359f,false}};
  Snapshot reference;unsigned cases=0;
  for(const auto& view:views) {
    r.width=view.width;r.height=view.height;
    world_renderer_prepare_draw(r,WorldCamera{-48,-20,90,view.yaw,0,1.03f});
    require((r.drawn_columns==0)==view.offscreen,"BT admission view fixture does not distinguish offscreen world");
    Cache cache(r);const auto actual=cache.fill(mesh.gpu.faces,unsigned(mesh.faces.size()),low,high,wanted);
    const float leaf_domain=-float(world_atlas_preview_layer(r.atlas,leaves)+1);
    for(int direction=0;direction<6;++direction) {
      const auto row=actual.find(Key{-44,-24,116,direction});
      require(row!=actual.end() && row->second[3]==leaf_domain,"BT resident leaf cache lost conditional layer metadata");
    }
    if(cases==0) {reference=actual;budget(f,cache);}
    else require(actual==reference,"BT world admission changed with camera direction or resolution");
    ++cases;
  }
  const Key clipped_high{-52,12,122,0};const auto clipped=expected(f,source,low,clipped_high);
  require(!clipped.empty() && clipped.size()<wanted.size(),"BT integer region fixture lacks a boundary");
  Cache clipped_cache(r);clipped_cache.fill(mesh.gpu.faces,unsigned(mesh.faces.size()),low,clipped_high,clipped);
  r.width=width;r.height=height;r.culling_enabled=culling;
  require(r.debug.errors.load()==0,"BT world admission graphics validation errors");
  std::printf("block_transport_admission=passed hardware=1 world_faces=1 gpu_surface_expansion=1 views=%u offscreen=1 resolution_independent=1 no_screen_resources=1 integer_region=1 conditional_leaf_metadata=1 rows=%zu validation_errors=0\n",cases,wanted.size());
}
}
