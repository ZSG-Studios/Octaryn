#pragma once
#include "Fsr2.h"
#include "TemporalCamera.h"
#include "TemporalResolution.h"
#include "TemporalTiming.h"
#include <array>
#include <chrono>
#include <slang-com-ptr.h>

namespace octaryn::client::rendering {
struct TemporalTargets {
  Slang::ComPtr<rhi::ITexture> opaque,object_motion,motion,reactive,output;
  Slang::ComPtr<rhi::ITextureView> opaque_view,object_view,motion_view,reactive_view,output_view;
};
struct WorldTemporal {
  Fsr2Context* fsr{};
  Slang::ComPtr<rhi::IComputePipeline> inputs;
  std::array<TemporalTargets,2> targets;
  TemporalCamera history;
  WorldCamera camera;
  unsigned mode{},requested_mode{},width{},height{},display_width{},display_height{};
  unsigned allocation_width{},allocation_height{};
  bool sharpening{true},dynamic_requested{},reconfigure{};
  float sharpness{.2f},custom_scale{.667f},minimum_scale{.5f},maximum_scale{1.f};
  unsigned target_fps{60};
  TemporalResolution resolution;
  TemporalTiming timing;
  Fsr2Jitter jitter;
  float delta_ms{16.6667f};
  bool reset{true};
  std::uint64_t reset_count{};
  using Clock=std::chrono::steady_clock;
  Clock::time_point last{},now{};
  ~WorldTemporal() {destroy_fsr2(fsr);}
};
void configure_temporal(WorldTemporal&,const WorldSceneSettings&,bool accept_mode);
void update_temporal_size(WorldTemporal&);
bool temporal_mode(WorldTemporal&,const char* name);
bool resize_temporal(WorldTemporal&,rhi::IDevice*,unsigned display_width,unsigned display_height,unsigned slots);
WorldCamera begin_temporal(WorldTemporal&,const WorldCamera&,std::uint64_t frame);
void commit_temporal(WorldTemporal&);
bool prepare_temporal(WorldTemporal&,rhi::ICommandEncoder*,unsigned slot,rhi::ITexture* depth,
    rhi::ITextureView* scene,rhi::ITextureView* material);
bool resolve_temporal(WorldTemporal&,rhi::ICommandEncoder*,unsigned slot,rhi::ITexture* depth,rhi::ITexture* scene);
}
