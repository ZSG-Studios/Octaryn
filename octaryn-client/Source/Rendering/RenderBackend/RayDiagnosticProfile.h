#pragma once
#include "RayDiagnosticMode.h"
#include "../../Diagnostics/AsyncProfileStream.h"
#include <slang-rhi.h>
#include <slang-rhi/shader-cursor.h>
#include <slang-com-ptr.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
namespace octaryn::client::rendering {
// Diagnostic-only atomics change shader timings; never mix these runs with FPS evidence.
class RayDiagnosticProfile {
  static constexpr unsigned count=33;
  struct Slot {
    Slang::ComPtr<rhi::IBuffer> gpu,readback;
    std::uint64_t frame{};
    bool pending{};
  };
  std::array<Slot,2> slots_;
  unsigned active_{};
  bool configured_{},enabled_{},wave_reported_{};
  RayDiagnosticMode mode_;
  diagnostics::AsyncProfileStream output_;
  bool resolve(rhi::IDevice* device,Slot& slot) {
    if(!slot.pending)return true;
    void* mapped{};
    if(SLANG_FAILED(device->mapBuffer(slot.readback,rhi::CpuAccessMode::Read,&mapped)) || !mapped)return false;
    std::array<std::uint32_t,count> values{};
    std::copy_n(static_cast<const std::uint32_t*>(mapped),count,values.begin());
    if(SLANG_FAILED(device->unmapBuffer(slot.readback)))return false;
    if(!values[21])values[23]=0;
    if(values[21] && !wave_reported_) {
      std::printf("reflection_wave_observed frame=%llu observations=%u minimum=%u maximum=%u source=gpu diagnostic=1\n",
        static_cast<unsigned long long>(slot.frame),values[21],values[23],values[22]);
      wave_reported_=true;
    }
    output_<<slot.frame;
    for(auto value:values)output_<<','<<value;
    output_<<",7\n";slot.pending=false;return bool(output_);
  }
public:
  bool configure(const char* option,const char* path,bool quiet_compiled=true) {
    if(configured_ || !resolve_ray_diagnostic_mode(option,path,mode_,quiet_compiled))return false;
    enabled_=mode_.collecting;
    if(enabled_) {
      const std::filesystem::path file(reinterpret_cast<const char8_t*>(mode_.path.c_str()));
      if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());
      output_.open(file);if(!output_)return false;
      output_<<"frame,shadow_receivers,shadow_history_rejected,shadow_queries,shadow_history_accepted,reflection_receivers,reflection_history_rejected,reflection_queries,reflection_visibility_queries,dynamic_reflection_hits,dynamic_reflection_blockers,negative_history_candidates,dynamic_reflection_receivers,dynamic_instance_start,max_reflection_instance,reflection_screen_attempts,reflection_screen_hits,reflection_screen_fallbacks,reflection_shaded_hits,reflection_unlit_hits,reflection_material_skipped,reflection_emission_only,reflection_wave_observations,reflection_wave_max,reflection_wave_min,reflection_witness_logical,reflection_witness_tests,reflection_witness_saved,reflection_witness_queries,reflection_quad_cells,reflection_quad_source_rays,reflection_quad_reconstructed,reflection_quad_full_rate,reflection_quad_published,schema_version\n";
    }
    configured_=true;return true;
  }
  const RayDiagnosticMode& mode() const {return mode_;}
  void report() const {
    std::printf("ray_diagnostic_mode requested=%s resolved=%u actual_compiled=%u collecting=%u cache_variant=%s frozen=device\n",
      mode_.requested.c_str(),unsigned(mode_.compiled),unsigned(mode_.compiled),unsigned(enabled_),
      ray_counter_cache_variant(mode_.compiled));
  }
  // Caller has retired this slot's frame fence before begin, including resize reuse.
  bool begin(rhi::IDevice* device,rhi::ICommandEncoder* commands,unsigned slot,std::uint64_t frame) {
    if(!configured_ || slot>=slots_.size())return false;
    if(!mode_.compiled)return true;
    active_=slot;auto& s=slots_[slot];
    if(!resolve(device,s))return false;
    if(!s.gpu) {
      rhi::BufferDesc desc{};desc.size=count*sizeof(std::uint32_t);desc.elementSize=sizeof(std::uint32_t);
      desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
      desc.defaultState=rhi::ResourceState::UnorderedAccess;desc.label="ray_diagnostic_counters";
      if(SLANG_FAILED(device->createBuffer(desc,nullptr,s.gpu.writeRef())))return false;
      if(enabled_) {
        desc.usage=rhi::BufferUsage::CopyDestination;desc.memoryType=rhi::MemoryType::ReadBack;
        desc.defaultState=rhi::ResourceState::CopyDestination;
        if(SLANG_FAILED(device->createBuffer(desc,nullptr,s.readback.writeRef())))return false;
      }
    }
    s.frame=frame;
    if(enabled_) {
      std::array<std::uint32_t,count> zero{};zero[23]=UINT32_MAX;
      if(SLANG_FAILED(commands->uploadBufferData(s.gpu,0,sizeof(zero),zero.data())))return false;
      commands->setBufferState(s.gpu,rhi::ResourceState::UnorderedAccess);
    }
    return true;
  }
  bool bind(rhi::IShaderObject* root) {
    rhi::ShaderCursor cursor(root);auto enabled=cursor["rayDiagnosticsEnabled"];
    const bool counters=cursor["rayDiagnosticCounters"].isValid();
    if(!enabled.isValid())return !counters;
    if(!configured_ || !mode_.compiled || !counters)return false;
    const unsigned flag=enabled_?1u:0u;
    return slots_[active_].gpu && SLANG_SUCCEEDED(enabled.setData(&flag,sizeof(flag))) &&
        SLANG_SUCCEEDED(cursor["rayDiagnosticCounters"].setBinding(rhi::Binding(slots_[active_].gpu)));
  }
  void end(rhi::ICommandEncoder* commands) {
    if(!enabled_)return;
    auto& s=slots_[active_];
    commands->copyBuffer(s.readback,0,s.gpu,0,count*sizeof(std::uint32_t));s.pending=true;
  }
  bool drain(rhi::IDevice* device) {
    // Caller has drained both frame fences.
    for(auto& slot:slots_)if(!resolve(device,slot))return false;
    if(output_)output_.flush();return !enabled_ || bool(output_);
  }
  bool close() {return output_.close();}
};
}
