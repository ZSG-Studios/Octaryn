#pragma once
#include <slang-rhi.h>
#include <array>
#include <fstream>
#include <filesystem>
#include <cstdint>
namespace octaryn::client::rendering {
enum class LightingPass : unsigned { Acceleration,DDGITrace,DDGIUpdate,LocalCull,LocalShade,SunTrace,SunFilter,Composition,Count };
class LightingProfile {
  static constexpr unsigned count=unsigned(LightingPass::Count);
  struct Slot { Slang::ComPtr<rhi::IQueryPool> pool;std::array<bool,count> written{};bool pending{};std::uint64_t frame{}; };
  std::array<Slot,2> slots_;
  unsigned active_{};double scale_{};std::ofstream file_;
public:
  bool initialize(rhi::IDevice* device,const char* path) {
    if(!path || !*path || !device->getInfo().timestampFrequency)return true;
    scale_=1000.0/double(device->getInfo().timestampFrequency);
    rhi::QueryPoolDesc desc{};desc.count=count*2;desc.label="lighting_pass_timings";
    for(auto& s:slots_)if(SLANG_FAILED(device->createQueryPool(desc,s.pool.writeRef())))return false;
    const std::filesystem::path target(path);if(target.has_parent_path())std::filesystem::create_directories(target.parent_path());
    file_.open(target);file_<<"frame,acceleration_ms,ddgi_trace_ms,ddgi_update_ms,local_cull_ms,local_shade_ms,sun_trace_ms,sun_filter_ms,composition_ms\n";
    return bool(file_);
  }
  bool resolve(unsigned slot) {
    auto& s=slots_[slot];if(!s.pending)return true;
    file_<<s.frame;
    for(unsigned i=0;i<count;++i) {
      double elapsed=0;
      if(s.written[i]) {
        std::uint64_t ticks[2]{};
        if(SLANG_FAILED(s.pool->getResult(i*2,2,ticks)) || ticks[1]<ticks[0])return false;
        elapsed=double(ticks[1]-ticks[0])*scale_;
      }
      file_<<','<<elapsed;
    }
    file_<<'\n';s.pending=false;return bool(file_);
  }
  bool begin(unsigned slot) {
    active_=slot;auto& s=slots_[slot];if(!s.pool)return true;
    s.written={};return SLANG_SUCCEEDED(s.pool->reset());
  }
  void begin_pass(rhi::ICommandEncoder* commands,LightingPass pass) {
    auto& s=slots_[active_];if(s.pool)commands->writeTimestamp(s.pool,unsigned(pass)*2);
  }
  void mark(rhi::ICommandEncoder* commands,LightingPass pass) {
    auto& s=slots_[active_];if(s.pool) {commands->writeTimestamp(s.pool,unsigned(pass)*2+1);s.written[unsigned(pass)]=true;}
  }
  void submit(std::uint64_t frame) {auto& s=slots_[active_];s.frame=frame;s.pending=bool(s.pool);}
  bool drain() {return resolve(0) && resolve(1);}
};
}
