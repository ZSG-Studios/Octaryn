#include "Probe.h"
#include "SlangShaderPath.h"
#include "AtlasInternal.h"
#include "AssetPath.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace mesh_probe {
namespace {
struct Ray {std::array<float,4> origin,direction;};
struct Result {
  std::array<std::uint32_t,4> identity;
  std::array<float,4> distance_normal,position,albedo,visibility;
};
static_assert(sizeof(Ray)==32 && sizeof(Result)==80);
constexpr unsigned Count=7;
// A signed-world cube occupies [-49,-48] x [-17,-16] x [111,112].
constexpr std::array<Ray,Count> Rays{{
    {{-48.5f,-10,111.5f,30},{0,-1,0,0}},
    {{-55,-16.5f,111.5f,8},{1,0,0,0}},
    {{-48.5f,-22,111.5f,30},{0,1,0,0}},
    {{-47.5f,-10,111.5f,30},{0,-1,0,0}},
    {{-48.5f,-10,111.5f,5},{0,-1,0,0}},
    {{-48.5f,-10,111.5f,30},{0,1,0,0}},
    {{-44.5f,-10,111.5f,30},{0,-1,0,0}}
}};
using Results=std::vector<Result>;
class Probe {
  WorldRenderer& r;
  WorldFrames frames;
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  Slang::ComPtr<rhi::IBuffer> input,lights;
  unsigned ray_count{};
  std::array<Slang::ComPtr<rhi::IBuffer>,2> output;
  std::uint64_t serial{};
public:
  explicit Probe(WorldRenderer& renderer,std::span<const Ray> rays=Rays):r(renderer),ray_count(static_cast<unsigned>(rays.size())) {
    require(frames.initialize(r.device,2),"ray probe two frame slots");
    require(r.capabilities.inline_lighting(),"central capabilities rejected the required ray-query device");
    require(world_ray_initialize(r) && world_ray_available(r),"ray probe requires actual hardware ray queries");
    const auto path=resolve_slang_shader_path("octaryn-client/Shaders/RayTracing/RayTracingProbe.slang");
    require(create_rhi_compute_pipeline(r.device,path.c_str(),"main",pipeline),"production ray query probe pipeline");
    input=buffer(r,rays.data(),rays.size_bytes(),sizeof(Ray),rhi::BufferUsage::ShaderResource);
    std::vector<WorldLocalLight> sources(rays.size());
    for(std::size_t i=0;i<rays.size();++i)for(unsigned axis=0;axis<3;++axis)
      sources[i].position_range[axis]=rays[i].origin[axis]+rays[i].direction[axis]*rays[i].origin[3];
    lights=buffer(r,sources.data(),sources.size()*sizeof(WorldLocalLight),sizeof(WorldLocalLight),rhi::BufferUsage::ShaderResource);
    rhi::BufferDesc desc{};desc.size=ray_count*sizeof(Result);desc.elementSize=sizeof(Result);
    desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
    desc.defaultState=rhi::ResourceState::UnorderedAccess;
    for(auto& result:output)checked(r.device->createBuffer(desc,nullptr,result.writeRef()),"ray probe result creation");
  }
  ~Probe() {frames.drain();r.queue->waitOnHost();}
  unsigned submit() {
    const auto slot=frames.slot(serial++);require(frames.wait(slot),"ray probe frame slot wait");
    r.active_frame=slot;r.frames=serial;
    auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"ray probe command encoder");
    require(prepare_player_shadows(r.player,commands,slot,r.player_pose,true),"production player shadow prepare");
    require(world_ray_prepare(r,commands,slot),"production ray scene prepare");
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"ray probe compute pass");
    auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"ray probe pipeline binding");
    require(world_ray_bind(r,root) && bind_world_atlas(r.atlas,root),"production ray scene and atlas bindings");
    const rhi::ShaderCursor cursor(root);
    checked(cursor["localLights"].setBinding(rhi::Binding(lights)),"ray local lights binding");
    checked(cursor["probeRays"].setBinding(rhi::Binding(input)),"ray input binding");
    checked(cursor["probeResults"].setBinding(rhi::Binding(output[slot])),"ray output binding");
    pass->dispatchCompute(ray_count,1,1);pass->end();
    auto command=commands->finish();require(command!=nullptr,"ray probe command finish");
    require(frames.submit(r.queue,command,slot),"ray probe frame submit");return slot;
  }
  Results read(unsigned slot) {
    require(frames.wait(slot),"ray probe readback frame wait");
    Results result(ray_count);
    checked(r.device->readBuffer(output[slot],0,result.size()*sizeof(Result),result.data()),"ray probe result readback");
    for(const auto& hit:result) {
      require(hit.identity[0]<=1,"ray hit flag invalid");
      for(const auto& values:{hit.distance_normal,hit.position,hit.albedo,hit.visibility})
        for(const auto value:values)require(std::isfinite(value),"ray query produced non-finite result");
    }
    return result;
  }
  Results settle(unsigned expected) {
    for(unsigned iteration=0;iteration<64;++iteration) {
      const auto result=read(submit());const auto stats=world_ray_stats(r);
      require(stats.active_jobs<=4,"ray BLAS build job bound exceeded");
      if(stats.ready_columns==expected && !stats.pending_columns && !stats.active_jobs)return result;
      // Qualification-only wait makes every next production zero-time poll deterministic.
      checked(r.queue->waitOnHost(),"ray probe build progress wait");
    }
    require(false,"ray scene did not settle within bounded fixture iterations");return {};
  }
};
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
    require(reference.opaque?(actual.visibility[3]>0 && actual.visibility[3]<1):actual.visibility[3]==1,
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
void player_shadow_case(Fixture& f) {
  auto& r=f.renderer;
  open_world_renderer_set_center(&r,40,40,0);
  char asset[4096]{};
  require(bundle_path_build(asset,sizeof(asset),"Client/Assets/Player/octaryn_player_v1.gltf"),"player shadow asset path");
  const auto shader=resolve_slang_shader_path("octaryn-client/Shaders/Player/Player.slang");
  require(!r.player,"player shadow fixture ownership");
  r.player=create_player_renderer(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,asset,shader.c_str());
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
  destroy_player_renderer(r.player);r.player=nullptr;
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
  checked(r.queue->waitOnHost(),"TLAS update fixture incoming build fence");
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
}
void ray_tracing_cases(Fixture& f) {
  journal_cases();
  auto& r=f.renderer;r.sources.clear();r.columns.clear();r.resident_quads=0;r.column_gpu_bytes=0;r.culling_enabled=true;
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
  vegetation_shadow_case(f);
  player_shadow_case(f);
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
