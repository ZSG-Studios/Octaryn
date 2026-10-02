#include "../MapWorld/MapTransformDiagnostics.h"
#include "WorldGeometry.h"
#include "GeometryRootUpload.h"
#include "GeometryStream.h"
#include "SelectionGpu.h"
#include "SelectionResources.h"
#include "SceneGeometryContext.h"
#include "InstanceSelection.h"
#include "GeometryBudget.h"
#include "HybridRenderer.h"
#include "WorldGeometryRaster.h"
#include "OcclusionGpu.h"
#include "../Rendering/RenderBackend/WorldRendererInternal.h"
#include "../Rendering/RenderBackend/SlangShaderPath.h"
#include "../MapWorld/MapRendererInternal.h"
#include "../MapWorld/MapTextureCache.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace octaryn::client::rendering::virtual_geometry {
static_assert(geometry_instance_view_reservation(1)==2*sizeof(InstanceSelectionView));
struct WorldGeometry::State {
  GeometryStream stream;
  SelectionGpu selection;
  OcclusionGpu occlusion;
  MapRenderer* map{};
  bool tiled{};
  std::vector<PageRequest> ray_feedback;
  HybridInputs inputs;
  SelectionGpuFrame frame;
  std::vector<PageRequest> feedback;
  std::vector<InstanceSelectionView> instance_views;
  Slang::ComPtr<rhi::IQueryPool> timing_pool[4];
  std::array<double,7> timing_ms{};
  double timing_scale{1};
  bool timing_pending[4]{},timing_enabled{};
  bool recorded{},selected_frame{},initialized{},reported_ready{},reported_selection{};
  bool admission_rejected{},root_cut{};
  float pixel_error{1},selected_error{};
  std::uint32_t selected{},feedback_overflow{};
  std::uint64_t pumps{};
  bool frame_diagnostics{};
  std::array<std::uint64_t,8> selection_frames{};
  std::string error;
  bool fail(const std::string& message) {error=message;std::fprintf(stderr,"world_geometry_failed reason=%s\n",message.c_str());return false;}
};
WorldGeometry::WorldGeometry():state_(std::make_unique<State>()) {}
WorldGeometry::~WorldGeometry()=default;
GeometryStream& WorldGeometry::stream() {return state_->stream;}
const GeometryAsset& WorldGeometry::asset() const {return state_->stream.asset();}
void WorldGeometry::request_ray_pages(std::span<const PageRequest> requests) {
  state_->ray_feedback.assign(requests.begin(),requests.end());
}
const std::string& WorldGeometry::error() const {return state_->error;}
bool WorldGeometry::admission_rejected() const {return state_->admission_rejected;}
bool WorldGeometry::complete_root_cut() const {return state_->root_cut;}
const SelectionGpuFrame& WorldGeometry::selection_frame() const {return state_->frame;}
bool WorldGeometry::ready() const {return state_->initialized && state_->stream.roots_ready();}
float WorldGeometry::requested_error_pixels() const {return state_->pixel_error;}
std::uint64_t WorldGeometry::gpu_bytes() const {
  const auto& s=*state_;std::uint64_t bytes=s.selection.gpu_bytes()+s.occlusion.gpu_bytes();
  if(auto* buffer=s.stream.clusters())bytes+=buffer->getDesc().size;
  if(!s.stream.shared_pool())if(auto* buffer=s.stream.pool())bytes+=buffer->getDesc().size;
  return bytes;
}
bool WorldGeometry::initialize(WorldRenderer& r,MapRenderer& map,std::shared_ptr<void> scheduler,const SceneGeometryContext* context) {
  auto& s=*state_;
  if(s.initialized || !r.capabilities.virtual_geometry() || map.geometry_cache.path.empty())
    return s.fail("virtual geometry requires a cooked map and supported mesh/atomic hardware");
  s.map=&map;s.tiled=r.tile_session!=nullptr || !map.geometry_instances.empty();
  if(const auto* value=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY_FRAME_DIAGNOSTICS"))
    s.frame_diagnostics=std::strcmp(value,"1")==0;
  const auto progress=[&](const char* stage) {world_renderer_load_stage(r,stage,false);};
  progress("Allocating world residency");
  GeometryStreamConfig config;config.slots=6144;config.feedback_capacity=8192;
  config.scheduler=std::move(scheduler);
  if(context) {
    if(!context->ledger || !context->pages || !context->selection)return s.fail("scene geometry context is incomplete");
    config.scene_pool=context->pages;config.scene_memory=context->ledger;
    config.feedback_capacity=context->selection->feedback_capacity(map.geometry_cache.pages);
  }
  if(const auto* text=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY_POOL_MIB");text && !s.tiled) {
    char* end{};const auto mib=std::strtoul(text,&end,10);
    if(end==text || *end || mib<8 || mib>1024)return s.fail("virtual geometry pool must be 8..1024 MiB");
    config.slots=static_cast<unsigned>(mib)*16;
  }
  if(const auto* text=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY_PIXELS")) {
    char* end{};s.pixel_error=std::strtof(text,&end);
    if(end==text || *end || !std::isfinite(s.pixel_error) || s.pixel_error<0 || s.pixel_error>4)
      return s.fail("virtual geometry error must be 0..4 pixels");
  }
  config.slots=std::min(config.slots,map.geometry_cache.pages);
  if(!context)config.feedback_capacity=std::max(config.feedback_capacity,map.geometry_cache.root_pages);
  if(!s.stream.initialize(r.device,map.geometry_cache.path,map.geometry_cache.hash,config)) {
    s.admission_rejected=s.stream.admission_rejected();return s.fail(s.stream.error());
  }
  if((s.stream.asset().space==GeometrySpace::Object)!=!map.geometry_instances.empty())
    return s.fail("object-space geometry requires explicit scene instances");
  if(s.stream.asset().material_count!=map.model.primitives.size())return s.fail("geometry material table does not match map");
  std::size_t root_clusters{};
  for(const auto root:s.stream.asset().roots)root_clusters+=s.stream.asset().groups[root].cluster_count;
  s.root_cut=root_clusters==s.stream.asset().clusters.size() && std::all_of(s.stream.asset().clusters.begin(),s.stream.asset().clusters.end(),
      [](const auto& cluster){return cluster.refined_group==invalid_id;});
  progress("Preparing world selection");
  SelectionTopology topology;
  if(!build_selection_topology(s.stream.asset(),topology,s.error))return s.fail(s.error);
  const auto shader=resolve_slang_shader_path("octaryn-client/Shaders/VirtualGeometry/Selection.slang");
  const auto directory=std::filesystem::path(shader).parent_path().generic_string();
  if(shader.empty())return s.fail("selection shader path is missing");
  if(!(context && s.root_cut) && !s.selection.initialize(r.device,topology,shader.c_str(),config.feedback_capacity,2,context?context->selection:nullptr)) {
    s.admission_rejected=s.selection.admission_rejected();return s.fail(s.selection.error());
  }
  progress("Preparing world occlusion");
  if(!s.tiled) {
    const auto occlusion_shader=resolve_slang_shader_path("octaryn-client/Shaders/VirtualGeometry/Occlusion.slang");
    if(occlusion_shader.empty() ||
        !s.occlusion.initialize(r.device,occlusion_shader.c_str(),static_cast<std::uint32_t>(s.stream.asset().clusters.size())))
      return s.fail(s.occlusion.error());
    // History culling false-culls under camera motion (flicker); opt-in until fixed.
    if(const auto* toggle=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY_OCCLUSION"))
      s.occlusion.set_history_enabled(std::strcmp(toggle,"1")==0);
    else s.occlusion.set_history_enabled(false);
  }
  if(const auto* toggle=std::getenv("OCTARYN_CLIENT_VIRTUAL_GEOMETRY_TIMING");toggle && std::strcmp(toggle,"0")!=0) {
    rhi::QueryPoolDesc timing{};timing.type=rhi::QueryType::Timestamp;timing.count=8;timing.label="world_geometry_timing";
    for(auto& pool:s.timing_pool)
      if(SLANG_FAILED(r.device->createQueryPool(timing,pool.writeRef())))return s.fail("geometry timing pool allocation failed");
    s.timing_enabled=r.device->getInfo().timestampFrequency!=0;
    s.timing_scale=1000.0/double(std::max<std::uint64_t>(r.device->getInfo().timestampFrequency,1));
    std::printf("world_geometry_timing_startup enabled=%u frequency=%llu\n",unsigned(s.timing_enabled),
        static_cast<unsigned long long>(r.device->getInfo().timestampFrequency));
  }
  progress("Preparing world raster");
  if(!r.geometry_raster)r.geometry_raster=std::make_unique<WorldGeometryRaster>();
  if(context) {
    if(!context->raster)return s.fail("scene raster tables are missing");
    if(r.geometry_raster->scene_tables && r.geometry_raster->scene_tables!=context->raster)return s.fail("scene raster table owner differs");
    r.geometry_raster->scene_tables=context->raster;
  }
  if(!r.geometry_raster->initialize(r,context!=nullptr))return s.fail("shared hybrid pipeline initialization failed");
  progress("Uploading world roots");
  const auto start=std::chrono::steady_clock::now();
  const auto root_progress=[&] {progress("Uploading world roots");};
  if(!s.tiled && !upload_geometry_roots(s.stream,r.device,r.queue,s.error,root_progress))return s.fail(s.error);
  if(!map_transform_diagnostics_snapshot(map))return s.fail("source transform diagnostic rejected invalid shader transform or bounds");
  s.initialized=true;
  const auto stats=s.stream.stats();
  s.reported_ready=s.stream.roots_ready();
  std::printf("world_geometry_initialized mode=required asset=%s clusters=%zu pages=%zu root_pages=%u slots=%u pixels=%.3f root_ms=%.3f transparency=sorted_forward rt=virtual_geometry\n",
      s.stream.asset().source_hash.c_str(),s.stream.asset().clusters.size(),s.stream.asset().pages.size(),stats.pinned_pages,
      unsigned(s.stream.pool()->getDesc().size/page_bytes),s.pixel_error,
      std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  if(s.reported_ready)std::printf("world_geometry_ready mode=required asset=%s clusters=%zu pages=%zu\n",s.stream.asset().source_hash.c_str(),s.stream.asset().clusters.size(),s.stream.asset().pages.size());
  return true;
}
bool WorldGeometry::stage_uploads(rhi::ICommandEncoder* commands) {
  auto& s=*state_;
  if(!s.initialized || s.recorded)return s.fail("geometry frame lifecycle invalid");
  s.feedback=std::move(s.ray_feedback);s.ray_feedback.clear();SelectionFeedback completed;
  while(s.selection.poll_feedback(completed)) {
    if(completed.selected_overflow || completed.missing_roots)return s.fail("GPU selected incomplete geometry cut");
    s.selected=completed.selected;s.selected_error=completed.maximum_error_pixels;s.feedback_overflow+=completed.feedback_overflow;
    if(s.frame_diagnostics && completed.slot<s.selection_frames.size())
      std::printf("world_geometry_feedback frame=%llu slot=%u generation=%u selected=%u used_pages=%zu requested_pages=%zu error_pixels=%.6g overflow=%u missing_roots=%u\n",
          static_cast<unsigned long long>(s.selection_frames[completed.slot]),completed.slot,completed.generation,
          completed.selected,completed.used_pages.size(),completed.requests.size(),completed.maximum_error_pixels,
          completed.feedback_overflow|completed.selected_overflow,completed.missing_roots);
    if(!s.reported_selection) {
      s.reported_selection=true;
      std::printf("world_geometry_selection asset=%s instance_union=%u instances=%zu selected=%u requested_pixels=%.6g error_pixels=%.6g\n",
          s.stream.asset().source_hash.c_str(),unsigned(!s.map->geometry_instances.empty()),s.map->geometry_instances.size(),
          s.selected,s.pixel_error,s.selected_error);
    }
    s.feedback.insert(s.feedback.end(),completed.requests.begin(),completed.requests.end());
    s.stream.touch_used(completed.used_pages);
  }
  if(!s.selection.error().empty())return s.fail(s.selection.error());
  if(!s.stream.pump(commands,s.feedback))return s.fail(s.stream.error());
  s.recorded=true;s.selected_frame=false;
  if(!s.stream.roots_ready())return true;
  if(!s.reported_ready) {
    s.reported_ready=true;
    std::printf("world_geometry_ready mode=required asset=%s clusters=%zu pages=%zu\n",s.stream.asset().source_hash.c_str(),s.stream.asset().clusters.size(),s.stream.asset().pages.size());
  }
  return true;
}
bool WorldGeometry::prepare(WorldRenderer& r,rhi::ICommandEncoder* commands,const WorldCamera& camera,std::size_t instance,bool selection_only) {
  auto& s=*state_;
  if(instance) {
    if(instance>=s.map->geometry_instances.size() || !s.recorded)return s.fail("geometry instance lifecycle invalid");
    if(!s.selected_frame)return true;
    s.inputs.transform=s.map->geometry_instances[instance];
    return r.geometry_raster->frames[r.active_frame].visibility(commands,s.inputs);
  }
  if(!stage_uploads(commands))return false;
  if(!s.stream.roots_ready())return true;
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
  if(!map_transform_diagnostics_frame(*s.map,r.frames,camera,view.frustum,s.selected,s.selected_error,s.pixel_error,selection_only?"scene_selection":"hybrid_visibility"))return s.fail("source transform diagnostic rejected invalid primary frame");
  s.instance_views.clear();s.instance_views.reserve(s.map->geometry_instances.size());
  for(const auto& transform:s.map->geometry_instances)s.instance_views.push_back(instance_selection_view(view,transform));
  const auto table=s.stream.page_table();
  const auto timing_slot=unsigned(s.pumps%4);
  auto* timing=s.timing_enabled?s.timing_pool[timing_slot].get():nullptr;
  if(timing && s.timing_pending[timing_slot]) {
    std::uint64_t ticks[8]{};
    if(SLANG_SUCCEEDED(timing->getResult(0,8,ticks))) {
      for(unsigned i=0;i<7;++i)s.timing_ms[i]=s.timing_ms[i]*.95+double(ticks[i+1]-ticks[i])*s.timing_scale*.05;
      timing->reset();s.timing_pending[timing_slot]=false;
    }
    else timing=nullptr;
  }
  else if(timing)timing->reset();
  if(timing)commands->writeTimestamp(timing,0);
  const bool selected=s.instance_views.empty()?s.selection.record(commands,table,view,s.frame,timing,1):
      s.selection.record(commands,table,s.instance_views,s.frame,timing,1);
  if(!selected)return s.fail(s.selection.error());
  if(s.frame.slot<s.selection_frames.size())s.selection_frames[s.frame.slot]=r.frames;
  if(s.frame_diagnostics) {
    const auto stats=s.stream.stats();
    std::printf("world_geometry_frame frame=%llu eye=%.9g,%.9g,%.9g yaw=%.9g pitch=%.9g resident=%u pending=%u loading=%u decoded_ready=%u uploaded_this_pump=%u\n",
        static_cast<unsigned long long>(r.frames),camera.x,camera.y,camera.z,camera.yaw,camera.pitch,
        stats.residency.resident,stats.residency.pending,stats.loading_pages,stats.ready_pages,stats.uploaded_this_pump);
  }
  s.inputs={s.stream.clusters(),s.stream.pool(),s.frame.page_table,s.frame.selected,s.frame.counters,s.map->ray_primitives,s.frame.dispatch,
      unsigned(r.render_width()),unsigned(r.render_height()),unsigned(s.stream.asset().clusters.size()),
      unsigned(s.stream.pool()->getDesc().size/page_bytes),r.view_uniforms,
      {r.lighting.skylight_floor,r.lighting.gameplay_sky_visibility,0,0}};
  s.inputs.material_range=s.map->material_buffer_range;
  if(!s.map->geometry_instances.empty())s.inputs.transform=s.map->geometry_instances.front();
  s.selected_frame=true;
  if(selection_only) {
    if(timing) {commands->writeTimestamp(timing,7);s.timing_pending[timing_slot]=true;}
    ++s.pumps;return true;
  }
  auto& hybrid=r.geometry_raster->frames[r.active_frame];
  if(!hybrid.resize(r.device,s.inputs.width,s.inputs.height))return s.fail("hybrid visibility resize failed");
  if(s.tiled) {
    if(!hybrid.visibility(commands,s.inputs))return s.fail("tiled hybrid visibility recording failed");
    if(timing) {commands->writeTimestamp(timing,7);s.timing_pending[timing_slot]=true;}
  } else {
    OcclusionInputs occlusion{s.inputs.clusters,s.frame.selected,s.frame.counters,
        s.inputs.width,s.inputs.height,s.inputs.view};
    if(!s.occlusion.begin(commands,r.active_frame,occlusion))return s.fail(s.occlusion.error());
    s.inputs.bin_args=s.occlusion.bin_args();
    s.inputs.software_bins=s.occlusion.early_software();s.inputs.hardware_bins=s.occlusion.early_hardware();
    s.inputs.bin_mesh_arg_offset=0;s.inputs.bin_software_arg_offset=12;
    if(!hybrid.visibility(commands,s.inputs,true))return s.fail("hybrid early visibility recording failed");
    if(!s.occlusion.build_current(commands,hybrid.visibility_buffer()) || !s.occlusion.retest(commands))
      return s.fail(s.occlusion.error());
    s.inputs.software_bins=s.occlusion.late_software();s.inputs.hardware_bins=s.occlusion.late_hardware();
    s.inputs.bin_mesh_arg_offset=24;s.inputs.bin_software_arg_offset=36;
    if(!hybrid.visibility(commands,s.inputs,false))return s.fail("hybrid late visibility recording failed");
    if(!s.occlusion.finish(commands,hybrid.visibility_buffer()))return s.fail(s.occlusion.error());
    if(timing) {commands->writeTimestamp(timing,7);s.timing_pending[timing_slot]=true;}
  }
  if(++s.pumps%120==0) {
    const auto stats=s.stream.stats();
    std::printf("world_geometry_stream asset=%s frame=%llu selected=%u resident_pages=%u pending_pages=%u gpu_bytes=%llu uploaded_bytes=%llu feedback_overflow=%u\n",
        s.stream.asset().source_hash.c_str(),static_cast<unsigned long long>(r.frames),s.selected,stats.residency.resident,stats.residency.pending,
        static_cast<unsigned long long>(gpu_bytes()),static_cast<unsigned long long>(stats.uploaded_bytes),s.feedback_overflow);
    std::printf("world_geometry_selection asset=%s instance_union=%u instances=%zu selected=%u requested_pixels=%.6g error_pixels=%.6g\n",
        s.stream.asset().source_hash.c_str(),unsigned(!s.instance_views.empty()),s.instance_views.size(),
        s.selected,s.pixel_error,s.selected_error);
    if(s.timing_enabled)
      std::printf("world_geometry_timing upload_ms=%.3f reset_ms=%.3f depthloop_ms=%.3f compact_ms=%.3f finish_ms=%.3f copies_ms=%.3f rest_ms=%.3f\n",
          s.timing_ms[0],s.timing_ms[1],s.timing_ms[2],s.timing_ms[3],s.timing_ms[4],s.timing_ms[5],s.timing_ms[6]);
  }
  return true;
}
bool WorldGeometry::resolve(WorldRenderer& r,rhi::IRenderPassEncoder* pass) {
  auto& s=*state_;return s.recorded && (!s.selected_frame || r.geometry_raster->frames[r.active_frame].resolve(pass,s.inputs));
}
bool WorldGeometry::submitted(rhi::IFence* fence,std::uint64_t value) {
  auto& s=*state_;if(!s.recorded)return true;
  if(!s.stream.submitted(fence,value) || (s.selected_frame && !s.selection.submitted(s.frame,fence,value)))
    return s.fail(s.stream.error().empty()?s.selection.error():s.stream.error());
  s.recorded=false;return true;
}
}
