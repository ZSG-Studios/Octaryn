#include "Probe.h"
#include "RayProbe.h"
#include "ResourceProbePlayer.h"
#include "SlangShaderPath.h"
#include "AtlasInternal.h"
#include "AssetPath.h"
#include "WorldRayBuildBudget.h"
#include "WorldRayTracingState.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mesh_probe {
namespace {
unsigned material(const Fixture& f,const char* name) {
  const std::string full=std::string("octaryn.basegame.block.")+name;
  for(unsigned i=1;i<f.catalog.size();++i)if(f.catalog[i].id==full)return i;
  require(false,"ray fixture material missing");return 0;
}
void hit(const Result& value,unsigned material,float distance,std::array<float,3> normal) {
  require(value.identity[0]==1 && value.identity[1]==material,"ray missed or returned wrong material");
  require(std::abs(value.distance_normal[0]-distance)<.001f,"ray triangle hit distance disagrees with cube oracle");
  for(unsigned axis=0;axis<3;++axis)
    require(std::abs(value.distance_normal[axis+1]-normal[axis])<.001f,"ray triangle normal disagrees with cube oracle");
  require(value.albedo[3]>.99f,"opaque ray material alpha changed");
}
void original(const Results& result,unsigned stone) {
  hit(result[0],stone,6,{0,1,0});hit(result[1],stone,6,{-1,0,0});hit(result[2],stone,5,{0,-1,0});
  for(unsigned i=3;i<Count;++i)require(result[i].identity[0]==0,"ray empty-space or finite-range oracle hit unexpectedly");
}
void moved(const Results& result,unsigned grass) {
  for(unsigned i=0;i<Count-1;++i)require(result[i].identity[0]==0,"edited ray scene retained stale cube geometry");
  hit(result[6],grass,6,{0,1,0});
}
void empty(const Results& result) {
  for(const auto& value:result)require(value.identity[0]==0,"unloaded or air ray scene retained stale geometry");
}
void event(const WorldRenderer& r,std::uint64_t cursor,SceneChangeKind kind,int x,int z) {
  bool found=false;
  require(r.scene_changes.for_each_since(cursor,[&](const SceneChange& change) {
    if(change.kind==kind && change.x==x && change.z==z)found=true;
  }),"fixture unexpectedly overflowed the scene-change journal");
  require(found,"geometry or AS publication failed to notify centralized scene changes");
}
void vegetation_shadow_case(Fixture& f) {
  auto& r=f.renderer;
  open_world_renderer_set_center(&r,0,0,0);
  auto source=column();
  const char* names[]={"bush","bluebell","gardenia","rose","lavender","white_torch"};
  constexpr unsigned species=6;
  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> atlas(load_atlas_rgba("Atlases/basegame-color.png"),SDL_DestroySurface);
  require(atlas && atlas->w==32*29 && atlas->h==32,"vegetation alpha reference atlas missing");
  std::vector<Ray> rays;
  struct Expected {unsigned material;bool opaque;};
  std::vector<Expected> expected;
  unsigned opaqueCount=0,holeCount=0;
  for(unsigned plant=0;plant<species;++plant) {
    const auto id=material(f,names[plant]),layer=world_atlas_preview_layer(r.atlas,id);
    const int x=2+int(plant)*4;
    put(source,x,8,16,static_cast<std::uint16_t>(id));
    unsigned solid=0,holes=0;
    for(unsigned ty=1;ty<31;++ty)for(unsigned tx=1;tx<31;++tx) {
      const auto* row=static_cast<const Uint8*>(atlas->pixels)+ty*static_cast<unsigned>(atlas->pitch);
      const bool opaque=row[(layer*32+tx)*4+3]>=.35f*255;
      solid+=opaque?1u:0u;holes+=opaque?0u:1u;
      // Both crossed planes share uv=(1-localX,1-localY). Invert the
      // half-texel inset so each ray samples an independent PNG texel center.
      const float px=float(x)+1-float(tx)/31,py=9-float(ty)/31;
      for(int side:{-1,1}) {
        rays.push_back({{px,py,16.5f+3.5f*float(side),7},{0,0,float(-side),0}});
        expected.push_back({id,opaque});
      }
    }
    require(solid>0 && holes>0,"vegetation alpha fixture lacks opaque texels or transparent holes");
    opaqueCount+=solid*2;holeCount+=holes*2;
  }
  source.blocks.compact();
  require(open_world_renderer_update(&r,source),"vegetation shadow mesh publication");
  Probe probe(r,rays);
  const auto results=probe.settle(1);
  for(std::size_t i=0;i<results.size();++i) {
    const auto& actual=results[i];const auto& reference=expected[i];
    require((actual.identity[0]!=0)==reference.opaque,"grass/flower ray silhouette differs from original atlas alpha");
    if(reference.opaque)require(actual.identity[1]==reference.material && (actual.identity[2]&2)!=0,
        "vegetation shadow ray returned the wrong sprite material");
    for(unsigned mode=0;mode<3;++mode)require(actual.visibility[mode]==(reference.opaque?0.f:1.f),
        "grass/flower any-hit, sun or local shadow failed alpha or two-sided visibility");
    const bool transmitted=reference.opaque?(actual.visibility[3]>0 && actual.visibility[3]<1):actual.visibility[3]==1;
    if(!transmitted) {
      const auto stats=world_ray_stats(r);const auto texel=(i/2)%900;
      std::printf("sky_transmission_mismatch species=%s tx=%zu ty=%zu side=%d material=%u flags=%u opaque=%u sky=%.9g ready=%u pending=%u active=%u\n",
          names[i/1800],texel%30+1,texel/30+1,i%2?1:-1,reference.material,actual.identity[2],reference.opaque?1u:0u,
          actual.visibility[3],stats.ready_columns,stats.pending_columns,stats.active_jobs);
      std::fflush(stdout);
    }
    require(transmitted,
        "sky transmission must distinguish cutout coverage from transparent atlas holes");
  }
  const auto stone=material(f,"stone");
  for(unsigned plant=0;plant<species;++plant)put(source,2+int(plant)*4,8,18,static_cast<std::uint16_t>(stone));
  ++source.revision;source.blocks.compact();
  require(open_world_renderer_update(&r,source),"sky wall behind cutout publication");
  const auto blocked=probe.settle(1);
  for(std::size_t i=0;i<blocked.size();++i) {
    require(blocked[i].visibility[3]==0,"opaque wall behind foliage/torch leaked sky transmission");
    // From -Z the alpha-covered sprite is nearer; from +Z the wall is nearer.
    const auto nearest=expected[i].opaque && i%2==0?expected[i].material:stone;
    require(blocked[i].identity[0]==1 && blocked[i].identity[1]==nearest,
        "sky fixture failed to exercise both blocker depth orders");
  }
  for(unsigned plant=0;plant<species;++plant)put(source,2+int(plant)*4,8,18,0);
  ++source.revision;source.blocks.compact();
  require(open_world_renderer_update(&r,source),"sky blocker removal publication");
  const auto restored=probe.settle(1);
  for(std::size_t i=0;i<restored.size();++i)
    require(std::abs(restored[i].visibility[3]-results[i].visibility[3])<1e-6f,
        "removed sky blocker did not restore original cutout transmission");
  std::printf("sky_transmission=passed production_query=1 species=%u torch=1 rays=%zu opaque_blocker_depth_orders=2 alpha_holes=1 wall_removal=1\n",
      species,rays.size());
  open_world_renderer_set_center(&r,40,40,0);empty(probe.settle(0));
  require(r.debug.errors.load()==0,"vegetation shadow validation errors");
  std::printf("vegetation_rt_shadows=passed species=%u directions=2 atlas_texels=%u rays=%zu opaque=%u holes=%u sun=1 local=1 any_hit=1\n",
      species,species*900,rays.size(),opaqueCount,holeCount);
}
void journal_cases() {
  SceneChanges journal;unsigned visited=0;
  journal.notify_column(-2,3,-32,32,SceneChangeKind::Modified);
  const auto check=[&](const SceneChange& change) {
    require(change.revision==1 && change.x==-2 && change.z==3 && change.min_y==-32 && change.height==32,
      "scene-change region metadata was corrupted");++visited;
  };
  require(journal.for_each_since(0,check) && journal.for_each_since(0,check) && visited==2,
    "independent temporal consumers cannot read the same scene revision");
  for(unsigned i=0;i<256;++i)journal.notify_column(0,0,0,32,SceneChangeKind::Modified);
  require(!journal.for_each_since(0,check),"expired temporal cursor did not request full invalidation");
  require(journal.for_each_since(journal.revision(),check) && visited==2,"current cursor replayed old edits");
}
void player_raster_shadow(WorldRenderer& r,bool perspective) {
  resource_probe::Frame frame("player_shadow_readback");
  constexpr unsigned size=Fixture::Size;
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"player raster commands");
  require(prepare_player_shadows(r.player,commands,0,r.player_pose,false),"raster-only player shadow preparation");
  rhi::RenderPassDepthStencilAttachment depth{};depth.view=r.targets[0].depth_view;
  depth.depthClearValue=1;depth.depthLoadOp=rhi::LoadOp::Clear;depth.depthStoreOp=rhi::StoreOp::Store;
  rhi::RenderPassDesc desc{};desc.depthStencilAttachment=&depth;
  auto* pass=commands->beginRenderPass(desc);require(pass!=nullptr,"player raster depth pass");
  rhi::RenderState state{};state.viewports[0]=rhi::Viewport::fromSize(float(size),float(size));state.viewportCount=1;
  state.scissorRects[0]=rhi::ScissorRect::fromSize(size,size);state.scissorRectCount=1;pass->setRenderState(state);
  const float center[4]={0,.9f,perspective?2.f:0.f,1},right[4]={1,0,0,0},up[4]={0,1,0,0},forward[4]={0,0,-1,0};
  const float projection[4]={4/3.99f,.04f/3.99f,0,perspective?.01f:0.f};
  require(render_player_shadow(r.player,pass,center,right,up,forward,projection),"full-body player depth draw");
  pass->end();auto command=commands->finish();require(command!=nullptr,"player raster finish");
  checked(r.queue->submit(command),"player raster submit");checked(r.queue->waitOnHost(),"player raster completion");
  Slang::ComPtr<ISlangBlob> pixels;rhi::SubresourceLayout layout{};
  checked(r.device->readTexture(r.targets[0].depth,0,0,pixels.writeRef(),&layout),"player shadow depth readback");
  require(pixels && layout.colPitch==sizeof(float) && layout.rowPitch>=size*sizeof(float),"player depth layout");
  unsigned written=0;
  for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x) {
    float value{};std::memcpy(&value,static_cast<const unsigned char*>(pixels->getBufferPointer())+y*layout.rowPitch+x*sizeof(float),sizeof(float));
    require(std::isfinite(value),"non-finite player shadow depth");if(value<1)++written;
  }
  require(written>100 && written<size*size/2,"player shadow depth silhouette coverage");
}
void player_shadow_case(Fixture& f,PlayerRenderer* prepared_player) {
  auto& r=f.renderer;
  open_world_renderer_set_center(&r,40,40,0);
  require(!r.player,"player shadow fixture ownership");
  r.player=prepared_player;
  require(r.player!=nullptr,"player shadow original glTF load");
  r.player_pose={};r.player_pose.first_person=true;
  // Cast horizontal rays through the authored head, torso, arms and legs.
  const std::array<Ray,7> rays{{
    {{0,1.65f,2,4},{0,0,-1,0}},{{0,1.1f,2,4},{0,0,-1,0}},
    {{.375f,1.1f,2,4},{0,0,-1,0}},{{-.375f,1.1f,2,4},{0,0,-1,0}},
    {{.125f,.35f,2,4},{0,0,-1,0}},{{-.125f,.35f,2,4},{0,0,-1,0}},
    {{1,1.1f,2,4},{0,0,-1,0}}
  }};
  Probe probe(r,rays);
  auto verify=[&](const Results& results,bool present) {
    for(unsigned i=0;i<results.size();++i) {
      require(results[i].identity[0]==0,"player shadow changed terrain material queries");
      const float expected=present && i<6?0.f:1.f;
      for(unsigned mode=0;mode<4;++mode)
        require(results[i].visibility[mode]==expected,"full-body player sun/local/any-hit/sky shadow mismatch");
    }
  };
  verify(probe.settle(0),true);
  const auto retained=probe.submit();
  r.player_pose.feet_x=4;r.player_pose.source_seconds=.1;
  const auto moved=probe.submit();
  verify(probe.read(retained),true);verify(probe.read(moved),false);
  r.player_pose.feet_x=0;r.player_pose.visible=false;
  verify(probe.read(probe.submit()),false);
  r.player_pose.visible=true;r.player_pose.source_seconds=0;
  verify(probe.read(probe.submit()),true);
  r.player_pose.first_person=false;
  verify(probe.read(probe.submit()),true);
  checked(r.queue->waitOnHost(),"player shadow frame cleanup");
  r.player_pose.first_person=true;player_raster_shadow(r,false);player_raster_shadow(r,true);
  r.player=nullptr;
  require(r.debug.errors.load()==0,"player shadow graphics validation errors");
  std::puts("player_rt_shadows=passed authored_limbs=6 silhouette_miss=1 first_person=1 third_person=1 sun=1 local=1 movement=1 hidden=1 retained_frames=2 raster_sun=1 raster_local=1");
}
void tlas_replacement_case(WorldRenderer& r,Probe& probe,unsigned stone) {
  open_world_renderer_set_center(&r,40,40,0);empty(probe.settle(0));
  open_world_renderer_set_center(&r,-1,3,1);
  auto a=column(-2,3,-32,32),b=column(-1,3,-32,32),c=column(0,3,-32,32);
  for(auto* source:{&a,&b,&c})put(*source,15,15,15,static_cast<std::uint16_t>(stone));
  require(open_world_renderer_update(&r,a) && open_world_renderer_update(&r,b),"TLAS update fixture initial columns");
  original(probe.settle(2),stone);
  require(open_world_renderer_update(&r,c),"TLAS update fixture incoming column");
  const auto retained=probe.submit();
  require(world_ray_stats(r).active_jobs==1 && world_ray_stats(r).ready_columns==2,
    "incoming BLAS was not scheduled independently of existing columns");
  probe.finish_builds();
  const auto before=world_ray_stats(r);
  // One completed arrival and one eviction preserve TLAS instance count.
  open_world_renderer_set_center(&r,0,3,1);
  original(probe.read(retained),stone);
  empty(probe.read(probe.submit()));
  const auto after=world_ray_stats(r);
  require(after.ready_columns==2 && after.tlas_updates==before.tlas_updates && after.tlas_builds==before.tlas_builds+1,
    "same-count streaming replacement must build exactly one immutable TLAS");
  require(after.blas_builds==before.blas_builds && after.blas_refits==before.blas_refits,
    "TLAS replacement rebuilt unrelated resident BLAS");
  open_world_renderer_set_center(&r,40,40,0);empty(probe.settle(0));
}
void tlas_reuse_case(Fixture& f,unsigned stone) {
  auto& r=f.renderer;
  open_world_renderer_set_center(&r,16,0,16);
  std::vector<Ray> rays;
  for(unsigned x=0;x<56;++x)rays.push_back({{float(x*32)+15.5f,22,15.5f,30},{0,-1,0,0}});
  const auto arrive=[&](unsigned x) {
    resource_probe::Frame frame("ray_arrival_mesh");
    auto source=column(int(x),0,0,32);put(source,15,15,15,static_cast<std::uint16_t>(stone));
    require(open_world_renderer_update(&r,source),"reuse streaming arrival mesh publication");
  };
  const auto verify=[&](const Results& results,unsigned first,unsigned last) {
    for(unsigned x=0;x<results.size();++x) {
      if(x>=first && x<=last)hit(results[x],stone,6,{0,1,0});
      else require(!results[x].identity[0],"reused TLAS leaked evicted or unpublished geometry");
    }
  };
  for(unsigned x=0;x<32;++x)arrive(x);
  Probe probe(r,rays);verify(probe.settle(32),0,31);
  auto previous=probe.submit();unsigned previous_first=0,previous_last=31;
  const auto before=world_ray_stats(r);
  auto& state=*r.ray_tracing->state;
  std::shared_ptr<world_ray::Snapshot> held;
  std::uint64_t held_generation{};rhi::IAccelerationStructure* held_tlas{};rhi::IBuffer* held_records{};
  for(unsigned step=1;step<=24;++step) {
    open_world_renderer_set_center(&r,16+int(step),0,16);arrive(31+step);
    const auto submitted=probe.submit();
    verify(probe.read(previous),previous_first,previous_last);
    verify(probe.read(submitted),step,30+step);
    require(world_ray_stats(r).active_jobs==1 && world_ray_stats(r).ready_columns==31,
      "reuse fixture failed to overlap one arrival with one eviction");
    if(step==4) {
      held=state.frames[submitted].snapshot;held_generation=held->generation;
      held_tlas=held->tlas;held_records=held->records;
    } else if(held) {
      require(held->generation==held_generation && held->tlas.get()==held_tlas && held->records.get()==held_records &&
          state.current.get()!=held.get() && state.current->tlas.get()!=held_tlas && state.current->records.get()!=held_records,
          "frame-slot reuse mutated an externally retained snapshot");
    }
    previous=submitted;previous_first=step;previous_last=30+step;
    probe.finish_builds();
  }
  const auto after=world_ray_stats(r);
  const auto builds=after.tlas_builds-before.tlas_builds;
  const auto allocations=after.tlas_allocations-before.tlas_allocations;
  const auto records=after.snapshot_record_allocations-before.snapshot_record_allocations;
  require(builds==24 && allocations<=4 && records<=4 && after.tlas_updates==before.tlas_updates,
      "continuous streaming did not reuse exclusive fenced TLAS/record capacity for full builds");
  verify(probe.settle(32),24,55);
  // Re-query the old GPU buffers after twenty newer scene generations.
  verify(probe.read(probe.submit(held)),4,34);held.reset();
  // Unchanged frames coalesce both slots onto current. The previous implementation
  // discarded the only reusable destination here and allocated on every edit.
  const auto idle_before=world_ray_stats(r);
  auto held_idle=state.current;
  const auto held_idle_generation=held_idle->generation;
  auto* held_idle_tlas=held_idle->tlas.get();auto* held_idle_records=held_idle->records.get();
  for(unsigned edit=0;edit<12;++edit) {
    const int y=edit%2?15:16;
    {
      resource_probe::Frame frame("ray_idle_edit_mesh");
      auto source=column(24,0,0,32);source.revision=edit+1;
      put(source,15,y,15,static_cast<std::uint16_t>(stone));
      require(open_world_renderer_update(&r,source),"idle reuse edited mesh publication");
    }
    const auto verify_idle=[&](const Results& result) {
      for(unsigned x=0;x<result.size();++x) {
        if(x>=24 && x<=55)hit(result[x],stone,x==24?float(21-y):6.f,{0,1,0});
        else require(!result[x].identity[0],"idle reuse leaked evicted geometry");
      }
    };
    verify_idle(probe.settle(32));
    const auto settled=world_ray_stats(r);
    for(unsigned idle=0;idle<2;++idle)verify_idle(probe.read(probe.submit()));
    const auto unchanged=world_ray_stats(r);
    require(unchanged.tlas_builds==settled.tlas_builds &&
        unchanged.tlas_allocations==settled.tlas_allocations &&
        unchanged.snapshot_record_allocations==settled.snapshot_record_allocations,
        "idle frames rebuilt or allocated unchanged ray scene");
    require(held_idle->generation==held_idle_generation && held_idle->tlas.get()==held_idle_tlas &&
        held_idle->records.get()==held_idle_records && state.current.get()!=held_idle.get(),
        "idle reuse mutated an externally retained snapshot");
    if(edit) {
      require(state.spare && state.spare.use_count()==1 && state.spare!=state.current &&
          state.spare->columns.empty() && !state.spare->map_blas,
          "idle frames lost spare capacity or retained obsolete geometry");
      require(state.frames[0].snapshot==state.current && state.frames[1].snapshot==state.current,
          "idle regression failed to coalesce frame snapshots");
      const auto capacity=[](const auto& s) {return s->tlas->getDesc().size+s->records->getDesc().size;};
      require(unchanged.tlas_bytes==capacity(state.current)+capacity(state.spare),
          "retained spare TLAS/records omitted or duplicated in byte accounting");
    }
  }
  const auto idle_after=world_ray_stats(r);
  const auto idle_builds=idle_after.tlas_builds-idle_before.tlas_builds;
  const auto idle_allocations=idle_after.tlas_allocations-idle_before.tlas_allocations;
  const auto idle_records=idle_after.snapshot_record_allocations-idle_before.snapshot_record_allocations;
  require(idle_builds==12 && idle_allocations<=2 && idle_records<=2 &&
      idle_after.tlas_updates==idle_before.tlas_updates,
      "intermittent scene changes reallocated TLAS/records after unchanged frames");
  verify(probe.read(probe.submit(held_idle)),24,55);held_idle.reset();
  std::printf("world_ray_idle_reuse=passed changes=12 idle_frames=24 full_builds=%llu "
      "tlas_allocations=%llu record_allocations=%llu spare_capacity=1 held_scene_requery=1 accounting=1\n",
      static_cast<unsigned long long>(idle_builds),static_cast<unsigned long long>(idle_allocations),
      static_cast<unsigned long long>(idle_records));
  open_world_renderer_set_center(&r,100,100,0);empty(probe.settle(0));
  empty(probe.read(probe.submit()));empty(probe.read(probe.submit()));
  require(world_ray_stats(r).retired_mesh_bytes==0 && !state.current->columns.size() && r.debug.errors.load()==0,
      "reused scene resources survived completed eviction frames or failed validation");
  std::printf("world_ray_reuse=passed arrivals=24 evictions=24 retained_generations=20 full_builds=%llu "
      "tlas_allocations=%llu record_allocations=%llu old_scene_requery=1 validation_errors=0\n",
      static_cast<unsigned long long>(builds),static_cast<unsigned long long>(allocations),static_cast<unsigned long long>(records));
}
}
void ray_tracing_cases(Fixture& f) {
  journal_cases();
  auto& r=f.renderer;r.sources.clear();r.columns.clear();r.resident_quads=0;r.column_gpu_bytes=0;r.culling_enabled=true;
  RayProbePipeline pipeline(r);ResourceProbePlayer prepared_player(r);
  require(world_ray_initialize(r) && world_ray_available(r),"ray fixture pre-frame initialization");
  require(prepare_world_atlas_plant_masks(r.atlas),"ray fixture pre-frame plant masks");
  r.player=prepared_player.get();r.player_pose={};player_raster_shadow(r,false);r.player=nullptr;
  resource_probe::start();
  const auto stone=material(f,"stone"),grass=material(f,"grass");
  auto source=column(-2,3,-32,32);put(source,15,15,15,static_cast<std::uint16_t>(stone));
  open_world_renderer_set_center(&r,-2,3,0);
  auto revision=r.scene_changes.revision();
  require(open_world_renderer_update(&r,source),"ray fixture initial production mesh publication");
  event(r,revision,SceneChangeKind::Added,-2,3);
  require(r.resident_quads==6,"ray cube fixture must contain exactly six production quads");
  Probe probe(r);original(probe.settle(1),stone);
  event(r,revision,SceneChangeKind::AccelerationReady,-2,3);
  const auto steady=world_ray_stats(r);
  require(steady.blas_builds==1 && steady.blas_bytes>0 && steady.tlas_bytes>0,"ray fixture never built real acceleration structures");
  for(unsigned frame=0;frame<4;++frame) {
    // The cube remains outside the camera view; rays must still reach it.
    world_renderer_prepare_draw(r,{1000+float(frame),200,1000,0,0,1.05f});
    require(r.drawn_columns==0,"ray fixture blocker is unexpectedly inside camera frustum");
    original(probe.read(probe.submit()),stone);
  }
  const auto stationary=world_ray_stats(r);
  require(stationary.blas_builds==steady.blas_builds && stationary.tlas_builds==steady.tlas_builds &&
      stationary.blas_refits==steady.blas_refits && stationary.tlas_updates==steady.tlas_updates,
      "camera-only movement rebuilt static ray geometry");
  const auto old0=probe.submit(),old1=probe.submit();
  require(old0!=old1,"ray query frames reused one mutable slot");
  // Eviction drops live mesh owners without waiting for either submitted frame.
  revision=r.scene_changes.revision();open_world_renderer_set_center(&r,40,40,0);
  event(r,revision,SceneChangeKind::Removed,-2,3);
  require(r.columns.empty() && r.sources.empty(),"ray frame-retention fixture did not evict live owners");
  original(probe.read(old0),stone);original(probe.read(old1),stone);
  empty(probe.settle(0));
  require(world_ray_stats(r).retired_mesh_bytes>0,"inflight snapshot retained mesh memory was omitted");
  empty(probe.read(probe.submit()));empty(probe.read(probe.submit()));
  require(world_ray_stats(r).retired_mesh_bytes==0,"retired meshes survived both completed frame slots");
  open_world_renderer_set_center(&r,-2,3,0);
  require(open_world_renderer_update(&r,source),"ray retained-frame fixture reload");
  original(probe.settle(1),stone);const auto before_edit=world_ray_stats(r);
  const auto retained_edit0=probe.submit(),retained_edit1=probe.submit();
  revision=r.scene_changes.revision();
  put(source,15,15,15,0);put(source,19,15,15,static_cast<std::uint16_t>(grass));++source.revision;
  require(open_world_renderer_update(&r,source),"ray fixture edited mesh publication");
  event(r,revision,SceneChangeKind::Modified,-2,3);
  original(probe.read(retained_edit0),stone);original(probe.read(retained_edit1),stone);
  const auto replacing=probe.submit();
  require(world_ray_stats(r).pending_columns==1,"edited AS must report pending replacement");
  original(probe.read(replacing),stone); // No whole-column hole while replacement builds.
  moved(probe.settle(1),grass);
  event(r,revision,SceneChangeKind::AccelerationReady,-2,3);
  const auto after_edit=world_ray_stats(r);
  // Production builds into a fresh immutable AS; an uninitialized destination
  // cannot be used as an update target. Retained frames above verify the old AS.
  require(after_edit.blas_builds==before_edit.blas_builds+1 && after_edit.blas_refits==before_edit.blas_refits,
    "same-count geometry edit must rebuild exactly its changed immutable BLAS");
  put(source,27,15,15,static_cast<std::uint16_t>(stone));++source.revision;
  require(open_world_renderer_update(&r,source) && r.resident_quads==12,"topology fixture must add six independent quads");
  moved(probe.settle(1),grass);
  const auto topology=world_ray_stats(r);
  require(topology.blas_builds==after_edit.blas_builds+1 && topology.blas_refits==after_edit.blas_refits,
    "changed primitive count incorrectly reused BLAS update topology");
  put(source,27,15,15,0);++source.revision;
  require(open_world_renderer_update(&r,source),"topology fixture restore");moved(probe.settle(1),grass);
  open_world_renderer_set_center(&r,40,40,0);empty(probe.settle(0));
  open_world_renderer_set_center(&r,-2,3,0);
  require(open_world_renderer_update(&r,source),"ray fixture reloaded mesh publication");
  moved(probe.settle(1),grass);
  put(source,19,15,15,0);++source.revision;
  require(open_world_renderer_update(&r,source),"ray fixture edited-air mesh publication");
  empty(probe.settle(0));
  require(r.columns.size()==1 && r.resident_quads==0,"ray edited air must remain a resident zero-face column");
  tlas_replacement_case(r,probe,stone);
  tlas_reuse_case(f,stone);
  vegetation_shadow_case(f);
  player_shadow_case(f,prepared_player.get());
  const auto final=world_ray_stats(r);
  require(!final.ready_columns && !final.pending_columns && !final.active_jobs,"ray empty scene accounting stale");
  require(r.debug.errors.load()==0,"ray fixture native validation errors");
  std::printf("world_ray_probe=passed hardware=1 production_mesh=1 exact_triangle_query=1 signed_coordinates=1 offscreen=1 "
      "finite_range=1 camera_rebuilds=0 retained_frames=2 edit=1 eviction=1 reload=1 edited_air=1 same_count_rebuild=1 "
      "topology_rebuild=1 tlas_stream_rebuild=1 scene_notifications=1 journal_overflow=1 retired_mesh_release=1 "
      "blas_builds=%llu tlas_builds=%llu blas_refits=%llu tlas_updates=%llu\n",
      static_cast<unsigned long long>(final.blas_builds),static_cast<unsigned long long>(final.tlas_builds),
      static_cast<unsigned long long>(final.blas_refits),static_cast<unsigned long long>(final.tlas_updates));
}
}
