#include "WorldGeometry.h"
#include "GeometryStream.h"
#include "SelectionGpu.h"
#include "HybridRenderer.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include "../Rendering/RenderBackend/SlangShaderPath.h"
#include "../MapWorld/MapRendererInternal.h"
#include "../MapWorld/MapTextureCache.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace octaryn::client::rendering::virtual_geometry {
bool world_geometry_requested() {
  const auto* path=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY");return path && *path;
}
struct WorldGeometry::State {
  GeometryStream stream;
  SelectionGpu selection;
  std::array<HybridRenderer,2> hybrid;
  HybridInputs inputs;
  SelectionGpuFrame frame;
  std::vector<PageRequest> feedback;
  bool recorded{},initialized{};
  float pixel_error{1};
  std::uint32_t selected{},feedback_overflow{};
  std::uint64_t pumps{};
  std::string error;
  bool fail(const std::string& message) {error=message;std::fprintf(stderr,"world_geometry_failed reason=%s\n",message.c_str());return false;}
};
WorldGeometry::WorldGeometry():state_(std::make_unique<State>()) {}
WorldGeometry::~WorldGeometry()=default;
const std::string& WorldGeometry::error() const {return state_->error;}
bool WorldGeometry::ready() const {return state_->initialized && state_->stream.roots_ready();}
std::uint64_t WorldGeometry::gpu_bytes() const {
  const auto& s=*state_;std::uint64_t bytes=s.selection.gpu_bytes();
  for(auto* buffer:{s.stream.pool(),s.stream.clusters(),s.hybrid[0].visibility_buffer(),s.hybrid[1].visibility_buffer()})
    if(buffer)bytes+=buffer->getDesc().size;
  return bytes;
}
bool WorldGeometry::initialize(WorldRenderer& r,const std::filesystem::path& source) {
  auto& s=*state_;
  if(s.initialized || !r.map || r.tile_session || !r.capabilities.virtual_geometry())
    return s.fail("virtual geometry requires supported hardware and one monolithic map");
  const auto* cache=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY");
  if(!cache || !*cache)return s.fail("virtual geometry cache path missing");
  GeometryStreamConfig config;config.slots=6144;config.feedback_capacity=8192;
  if(const auto* text=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY_POOL_MIB")) {
    char* end{};const auto mib=std::strtoul(text,&end,10);
    if(end==text || *end || mib<8 || mib>1024)return s.fail("virtual geometry pool must be 8..1024 MiB");
    config.slots=static_cast<unsigned>(mib)*16;
  }
  if(const auto* text=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY_PIXELS")) {
    char* end{};s.pixel_error=std::strtof(text,&end);
    if(end==text || *end || !std::isfinite(s.pixel_error) || s.pixel_error<0 || s.pixel_error>4)
      return s.fail("virtual geometry error must be 0..4 pixels");
  }
  const auto hash=map_texture_file_digest(source,s.error);
  if(hash.empty() || !s.stream.initialize(r.device,std::filesystem::u8path(cache),hash,config))
    return s.fail(hash.empty()?s.error:s.stream.error());
  if(s.stream.asset().space!=GeometrySpace::World)return s.fail("monolithic map requires world-space geometry cache");
  if(s.stream.asset().material_count!=r.map->model.primitives.size())return s.fail("geometry material table does not match map");
  SelectionTopology topology;
  if(!build_selection_topology(s.stream.asset(),topology,s.error))return s.fail(s.error);
  const auto shader=resolve_slang_shader_path("octaryn-client/Shaders/VirtualGeometry/Selection.slang");
  const auto directory=std::filesystem::path(shader).parent_path().generic_string();
  if(shader.empty() || !s.selection.initialize(r.device,topology,shader.c_str(),config.feedback_capacity,2))return s.fail(s.selection.error());
  const auto targets=std::span(world_gbuffer_formats).first(world_gbuffer_attachment_count(r.device));
  for(auto& hybrid:s.hybrid)
    if(!hybrid.initialize(r.device,directory.c_str(),targets,rhi::Format::D32Float))return s.fail("hybrid pipeline initialization failed");
  const auto start=std::chrono::steady_clock::now();
  while(!s.stream.roots_ready()) {
    if(std::chrono::steady_clock::now()-start>std::chrono::seconds(60))return s.fail("coarse geometry startup timed out");
    auto commands=r.queue->createCommandEncoder();if(!commands)return s.fail("root upload encoder failed");
    if(!s.stream.pump(commands,{}))return s.fail(s.stream.error());
    auto submission=commands->finish();
    if(!submission || !r.frame_queue.submit(r.queue,submission,0))return s.fail("root upload submission failed");
    if(!s.stream.submitted(r.frame_queue.fence(),r.frame_queue.last_signal()))return s.fail(s.stream.error());
    if(!r.frame_queue.wait(0,frame_fence_timeout_ms()))return s.fail("root upload fence timed out");
    if(!s.stream.roots_ready())SDL_Delay(1);
  }
  s.initialized=true;
  const auto stats=s.stream.stats();
  std::printf("world_geometry_ready mode=opt_in_monolithic clusters=%zu pages=%zu root_pages=%u slots=%u pixels=%.3f root_ms=%.3f transparency=existing_forward rt=existing_full_detail\n",
      s.stream.asset().clusters.size(),s.stream.asset().pages.size(),stats.pinned_pages,config.slots,s.pixel_error,
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  return true;
}
bool WorldGeometry::prepare(WorldRenderer& r,rhi::ICommandEncoder* commands,const WorldCamera& camera) {
  auto& s=*state_;if(!s.initialized || s.recorded)return s.fail("geometry frame lifecycle invalid");
  s.feedback.clear();SelectionFeedback completed;
  while(s.selection.poll_feedback(completed)) {
    if(completed.selected_overflow || completed.missing_roots)return s.fail("GPU selected incomplete geometry cut");
    s.selected=completed.selected;s.feedback_overflow+=completed.feedback_overflow;
    s.feedback.insert(s.feedback.end(),completed.requests.begin(),completed.requests.end());
  }
  if(!s.selection.error().empty())return s.fail(s.selection.error());
  if(!s.stream.pump(commands,s.feedback))return s.fail(s.stream.error());
  s.recorded=true;
  SelectionView view{{camera.x,camera.y,camera.z},
      .5f*float(r.render_height())/std::tan(std::clamp(camera.vertical_fov,.2f,2.7f)*.5f),s.pixel_error};
  // Derive camera-relative frustum planes in world coordinates. Expand by one
  // pixel to cover temporal jitter without per-frame boundary popping.
  const auto& basis=r.view_uniforms;
  const float sx=basis[16]/(1+2.f/float(r.render_width()));
  const float sy=basis[17]/(1+2.f/float(r.render_height()));
  for(unsigned plane=0;plane<4;++plane) {
    const unsigned axis=plane<2?4:8;const float sign=plane%2?1.f:-1.f;
    for(unsigned i=0;i<3;++i)view.planes[plane][i]=basis[12+i]+sign*basis[axis+i]*(plane<2?sx:sy);
  }
  for(unsigned i=0;i<3;++i) {view.planes[4][i]=basis[12+i];view.planes[5][i]=-basis[12+i];}
  for(unsigned p=0;p<6;++p) {
    view.planes[p][3]=-view.planes[p][0]*camera.x-view.planes[p][1]*camera.y-view.planes[p][2]*camera.z;
    if(p==4)view.planes[p][3]-=.1f;else if(p==5)view.planes[p][3]+=8192;
  }
  view.frustum=r.culling_enabled;
  const auto table=s.stream.page_table();
  if(!s.selection.record(commands,table,view,s.frame))return s.fail(s.selection.error());
  s.inputs={s.stream.clusters(),s.stream.pool(),s.frame.page_table,s.frame.selected,s.frame.counters,r.map->ray_primitives,s.frame.dispatch,
      unsigned(r.render_width()),unsigned(r.render_height()),unsigned(s.stream.asset().clusters.size()),
      unsigned(s.stream.pool()->getDesc().size/page_bytes),r.view_uniforms,
      {r.lighting.skylight_floor,r.lighting.gameplay_sky_visibility,0,0}};
  auto& hybrid=s.hybrid[r.active_frame];
  if(!hybrid.resize(r.device,s.inputs.width,s.inputs.height) || !hybrid.visibility(commands,s.inputs))
    return s.fail("hybrid visibility recording failed");
  if(++s.pumps%120==0) {
    const auto stats=s.stream.stats();
    std::printf("world_geometry_stream frame=%llu selected=%u resident_pages=%u pending_pages=%u gpu_bytes=%llu uploaded_bytes=%llu feedback_overflow=%u\n",
        static_cast<unsigned long long>(r.frames),s.selected,stats.residency.resident,stats.residency.pending,
        static_cast<unsigned long long>(gpu_bytes()),static_cast<unsigned long long>(stats.uploaded_bytes),s.feedback_overflow);
  }
  return true;
}
bool WorldGeometry::resolve(WorldRenderer& r,rhi::IRenderPassEncoder* pass) {
  auto& s=*state_;return s.recorded && s.hybrid[r.active_frame].resolve(pass,s.inputs);
}
bool WorldGeometry::submitted(rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;if(!s.recorded)return true;
  if(!s.stream.submitted(fence,value) || !s.selection.submitted(s.frame,fence,value))
    return s.fail(s.stream.error().empty()?s.selection.error():s.stream.error());
  s.recorded=false;return true;
}
}
