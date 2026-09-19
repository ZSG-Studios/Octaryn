#pragma once
#include <slang-rhi.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace octaryn::client::rendering {
// Each slot is resolved only after its owning frame fence, before reuse.
struct DDGITiming {
  struct Slot { Slang::ComPtr<rhi::IQueryPool> pool;unsigned work{},probes{},target{};std::uint64_t frame{}; };
  std::array<Slot,2> slots;
  double milliseconds_per_tick{};
  double trace_ms{},update_ms{};
  unsigned resolved_probes{},resolved_target{};std::uint64_t resolved_frame{};
  std::ofstream profile;
  bool initialize(rhi::IDevice* device,const char* path=nullptr,bool fine=false) {
    *this={};
    const auto frequency=device->getInfo().timestampFrequency;
    if(!frequency)return true;
    milliseconds_per_tick=1000./double(frequency);
    rhi::QueryPoolDesc desc{};desc.count=4;desc.label="ddgi_schedule_timings";
    for(auto& slot:slots)if(SLANG_FAILED(device->createQueryPool(desc,slot.pool.writeRef())))return false;
    if(path && *path) {
      const std::filesystem::path target=std::string(path)+(fine?".fine.csv":".coarse.csv");
      if(target.has_parent_path())std::filesystem::create_directories(target.parent_path());
      profile.open(target);
      if(!profile)return false;
      profile<<"source_frame,resolve_frame,slot,probes,work,trace_ms,update_ms,time_seconds,frame_seconds,budget_before,budget_after,ms_per_work,pending,oldest_seconds,credit,target_probes,partial_batch,gpu_debt_seconds\n";
    }
    return true;
  }
  bool resolve(unsigned index,double& milliseconds,unsigned& work) {
    auto& slot=slots[index];work=slot.work;milliseconds=trace_ms=update_ms=0;
    resolved_probes=slot.probes;resolved_target=slot.target;resolved_frame=slot.frame;
    if(!slot.pool || !work)return true;
    std::uint64_t ticks[4]{};
    if(SLANG_FAILED(slot.pool->getResult(0,4,ticks)))return false;
    if(ticks[1]>=ticks[0] && ticks[3]>=ticks[2]) {
      trace_ms=double(ticks[1]-ticks[0])*milliseconds_per_tick;
      update_ms=double(ticks[3]-ticks[2])*milliseconds_per_tick;
      milliseconds=trace_ms+update_ms;
    }
    slot.work=0;return true;
  }
  bool begin(rhi::ICommandEncoder* commands,unsigned index,bool trace) {
    auto& slot=slots[index];if(!slot.pool)return true;
    if(trace && SLANG_FAILED(slot.pool->reset()))return false;
    commands->writeTimestamp(slot.pool,trace?0:2);return true;
  }
  void end(rhi::ICommandEncoder* commands,unsigned index,bool trace,unsigned work,unsigned probes,unsigned target,std::uint64_t frame) {
    auto& slot=slots[index];if(!slot.pool)return;
    commands->writeTimestamp(slot.pool,trace?1:3);
    if(!trace) {slot.work=work;slot.probes=probes;slot.target=target;slot.frame=frame;}
  }
};
}
