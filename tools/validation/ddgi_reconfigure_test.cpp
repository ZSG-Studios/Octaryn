#include "DDGIVolumeConfig.h"
#include "LightingQuality.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace octaryn::client::rendering {
// Only resource creation and host discovery are mocked; reconfiguration is production code.
class Buffer final:public rhi::IBuffer {
  unsigned references{1};
  rhi::BufferDesc desc;
public:
  SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID&,void**) override {return SLANG_E_NOT_IMPLEMENTED;}
  SLANG_NO_THROW std::uint32_t SLANG_MCALL addRef() override {return ++references;}
  SLANG_NO_THROW std::uint32_t SLANG_MCALL release() override {const auto left=--references;if(!left)delete this;return left;}
  SLANG_NO_THROW rhi::Result SLANG_MCALL getNativeHandle(rhi::NativeHandle*) override {return SLANG_E_NOT_IMPLEMENTED;}
  SLANG_NO_THROW const rhi::BufferDesc& SLANG_MCALL getDesc() override {return desc;}
  SLANG_NO_THROW rhi::Result SLANG_MCALL getSharedHandle(rhi::NativeHandle*) override {return SLANG_E_NOT_IMPLEMENTED;}
  SLANG_NO_THROW rhi::DeviceAddress SLANG_MCALL getDeviceAddress() override {return 0;}
  SLANG_NO_THROW rhi::Result SLANG_MCALL getDescriptorHandle(rhi::DescriptorHandleAccess,rhi::Format,rhi::BufferRange,
    rhi::DescriptorHandle*) override {return SLANG_E_NOT_IMPLEMENTED;}
};
struct WorldRenderer {
  DDGISystem ddgi;
  LightingSettings lighting_settings;
  struct Local {struct Settings {unsigned tile_capacity{},debug{};} settings;} local_lighting;
  struct Changes {std::uint64_t revision() const {return 42;}} scene_changes;
  unsigned allocations{};
  WorldRenderer() {lighting_settings.ddgi_voxel_radius=16;}
};
static bool world_ray_available(WorldRenderer&) {return true;}
static const char* SDL_getenv(const char*) {return nullptr;}
static void ddgi_clear_ignore(DDGISystem&) {}
static bool initialize_volume(WorldRenderer& r,DDGISystem& s) {
  ++r.allocations;s.controls.attach(new Buffer);
  if(!s.available)s.config.counts={1,1,1};
  const auto count=s.config.counts[0]*s.config.counts[1]*s.config.counts[2];
  s.control_data.resize(count);s.last_updates.resize(count);s.dirty.resize(count);
  return true;
}
}
#include "DDGIReconfigureUnderTest.h"
using namespace octaryn::client::rendering;
static void require(bool condition,const char* message) {
  if(!condition)throw std::runtime_error(message);
}
int main() {
  WorldRenderer r;
  require(world_ddgi_reconfigure(r) && r.allocations==2,"initial volumes did not allocate independently");
  auto* fine=r.ddgi.fine_volume.get();auto* fine_buffer=fine->controls.get();
  fine->frame=900;fine->initialized=true;fine->time_seconds=15;fine->last_updates[0]=899;
  fine->control_data[0].refresh_frame=898;fine->control_data[0].version=12;
  r.lighting_settings.ddgi_coarse_radius=1024;
  require(world_ddgi_reconfigure(r) && r.allocations==3,"coarse range reallocated the unchanged fine volume");
  require(fine==r.ddgi.fine_volume.get() && fine->controls.get()==fine_buffer && fine->frame==900 &&
    fine->time_seconds==15 && fine->last_updates[0]==899 && fine->control_data[0].refresh_frame==898 &&
    fine->control_data[0].version==12,"coarse range discarded mature fine observations or refresh markers");
  require(r.ddgi.config.spacing==64 && r.ddgi.config.max_distance==2048,"far coarse configuration did not reach allocation");
  require(world_ddgi_reconfigure(r) && r.allocations==3,"identical configuration reallocated volumes");
  r.ddgi.frame=700;auto* coarse_buffer=r.ddgi.controls.get();
  r.lighting_settings.ddgi_voxel_radius=6;
  require(world_ddgi_reconfigure(r) && r.allocations==4 && r.ddgi.frame==700 &&
    r.ddgi.controls.get()==coarse_buffer,"fine range discarded unrelated coarse state");
  r.lighting_settings.ddgi_voxel_radius=0;
  require(world_ddgi_reconfigure(r) && !r.ddgi.fine_volume && r.allocations==4,"fine disable reallocated coarse state");
  r.lighting_settings.ddgi_voxel_radius=16;
  require(world_ddgi_reconfigure(r) && r.ddgi.fine_volume && r.allocations==5,"fine enable failed");
  fine=r.ddgi.fine_volume.get();fine->frame=800;
  r.lighting_settings.ddgi_coarse_radius=0;
  require(world_ddgi_reconfigure(r) && !r.ddgi.available && r.allocations==6 && fine->frame==800,
    "coarse disable reset the fine volume");
  require(world_ddgi_reconfigure(r) && r.allocations==6,"disabled placeholder reallocated repeatedly");
  for(unsigned quality:{0u,1u,2u,3u,0u}) {
    const auto retained=fine->controls.get();
    open_world_renderer_set_lighting_quality(&r,quality);
    const double target=ddgi_quality_milliseconds(static_cast<LightingQuality>(quality));
    require(fine->config.gpu_budget_milliseconds==target && r.ddgi.config.gpu_budget_milliseconds==target &&
      r.ddgi.base_config.gpu_budget_milliseconds==target,"runtime quality did not reach both volumes and future allocations");
    for(unsigned frame=0;frame<1000;++frame)open_world_renderer_set_lighting_quality(&r,quality);
    require(world_ddgi_reconfigure(r) && r.allocations==6 && fine->frame==800 && fine->controls.get()==retained,
      "quality changes reallocated resources or discarded history");
  }
  std::puts("ddgi_reconfigure_test=passed independent_allocation=1 retained_fine_history=1 retained_coarse_history=1 disable_enable=1 runtime_quality=4 repeated_quality_no_allocation=1");
}
