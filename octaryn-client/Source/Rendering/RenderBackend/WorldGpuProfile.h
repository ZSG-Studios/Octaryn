#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace octaryn::client::rendering {
// Timestamp pools and CPU metadata share the renderer's two fence slots.
class WorldGpuProfile {
  static constexpr std::uint32_t MarkerCount=9;
  struct Record {
    std::uint64_t frame{};
    unsigned frame_count{};
    int width{},height{};
  };
  struct Slot {
    Slang::ComPtr<rhi::IQueryPool> queries;
    std::array<double,8> cpu{};
    Record record;
    bool pending{};
  };
  std::array<Slot,2> slots_;
  std::ofstream output_;
  std::uint32_t index_{};
  unsigned active_{};
  double milliseconds_per_tick_{};
  using Clock=std::chrono::steady_clock;
  Clock::time_point cpu_start_{};
  std::size_t cpu_index_{};
  bool failure(const char* stage,unsigned slot,std::int64_t actual,std::uint64_t expected=0) const {
    const auto frame=slot<slots_.size()?slots_[slot].record.frame:0;
    std::fprintf(stderr,"world_gpu_profile_failed stage=%s slot=%u frame=%llu actual=%lld expected=%llu\n",
        stage,slot,static_cast<unsigned long long>(frame),static_cast<long long>(actual),
        static_cast<unsigned long long>(expected));
    std::fflush(stderr);return false;
  }
public:
  WorldGpuProfile(rhi::IDevice* device,const char* path) {
    const auto frequency=device->getInfo().timestampFrequency;
    if(!frequency)throw std::runtime_error("GPU profiling requires device timestamps");
    milliseconds_per_tick_=1000.0/static_cast<double>(frequency);
    rhi::QueryPoolDesc desc{};desc.count=MarkerCount;desc.label="world_pass_timings";
    for(auto& slot:slots_)if(SLANG_FAILED(device->createQueryPool(desc,slot.queries.writeRef())))
      throw std::runtime_error("Cannot create world GPU timestamp pool");
    const std::filesystem::path file(reinterpret_cast<const char8_t*>(path));
    if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());
    output_.open(file);
    if(!output_)throw std::runtime_error("Cannot open GPU profile output");
    output_<<"frame,sky_ms,opaque_ms,hdr_ms,forward_ms,fsr_ms,tonemap_ms,ui_ms,copy_ms,total_gpu_ms,atlas_cpu_ms,acquire_cpu_ms,prepare_cpu_ms,encode_cpu_ms,submit_cpu_ms,present_cpu_ms,wait_cpu_ms,width,height,frames_in_flight\n";
    output_<<std::fixed<<std::setprecision(6);
  }
  bool resolve(unsigned index) {
    if(index>=slots_.size())return failure("resolve_slot",index,index,slots_.size());
    auto& slot=slots_[index];if(!slot.pending)return true;
    rhi::QueryResultState state{};
    const auto state_result=slot.queries->getResultState(0,MarkerCount,&state);
    if(SLANG_FAILED(state_result))return failure("query_state_result",index,state_result);
    if(state==rhi::QueryResultState::Reset)return failure("query_state",index,
        static_cast<std::int64_t>(state),static_cast<std::uint64_t>(rhi::QueryResultState::Resolved));
    std::array<std::uint64_t,MarkerCount> ticks{};
    // DX12 signals the caller's frame fence before RHI's tracking fence.
    // Pending here is valid: getResult waits only this query submission.
    const auto result=slot.queries->getResult(0,MarkerCount,ticks.data());
    if(SLANG_FAILED(result))return failure("query_result",index,result);
    const auto& r=slot.record;
    output_<<r.frame;
    for(std::size_t i=1;i<ticks.size();++i) {
      if(ticks[i]<ticks[i-1]) {
        std::fprintf(stderr,"world_gpu_profile_timestamp marker=%zu previous=%llu current=%llu\n",i,
            static_cast<unsigned long long>(ticks[i-1]),static_cast<unsigned long long>(ticks[i]));
        return failure("descending_timestamp",index,static_cast<std::int64_t>(i));
      }
      output_<<','<<static_cast<double>(ticks[i]-ticks[i-1])*milliseconds_per_tick_;
    }
    output_<<','<<static_cast<double>(ticks.back()-ticks.front())*milliseconds_per_tick_;
    for(const auto value:slot.cpu)output_<<','<<value;
    output_<<','<<r.width<<','<<r.height<<','<<r.frame_count<<'\n';
    if(std::getenv("OCTARYN_CLIENT_LIVE_FRAME_TIMING"))output_.flush();
    slot.pending=false;
    return output_?true:failure("output_write",index,output_.rdstate());
  }
  bool drain() {
    // Preserve submission order when stopping on an odd frame or resizing.
    while(slots_[0].pending || slots_[1].pending) {
      const auto first=slots_[0].pending && (!slots_[1].pending || slots_[0].record.frame<slots_[1].record.frame)?0u:1u;
      if(!resolve(first))return false;
    }
    return true;
  }
  void begin_cpu(unsigned index,double wait_ms) {
    if(index>=slots_.size() || slots_[index].pending)throw std::runtime_error("GPU profile slot reused before completion");
    active_=index;slots_[index].cpu={};slots_[index].cpu[7]=wait_ms;
    cpu_index_=0;cpu_start_=Clock::now();
  }
  void mark_cpu() {
    if(cpu_index_>=7)throw std::runtime_error("Too many world CPU timing markers");
    const auto now=Clock::now();
    slots_[active_].cpu[cpu_index_++]=std::chrono::duration<double,std::milli>(now-cpu_start_).count();
    cpu_start_=now;
  }
  void add_wait(double milliseconds) {slots_[active_].cpu[7]+=milliseconds;}
  bool begin(rhi::ICommandEncoder* commands) {
    const auto result=slots_[active_].queries->reset();
    if(SLANG_FAILED(result))return failure("query_reset",active_,result);
    index_=0;mark(commands);return true;
  }
  void mark(rhi::ICommandEncoder* commands) {
    if(index_>=MarkerCount)throw std::runtime_error("Too many world GPU timestamp markers");
    commands->writeTimestamp(slots_[active_].queries,index_++);
  }
  bool finish(std::uint64_t frame,int width,int height,unsigned frame_count) {
    if(index_!=MarkerCount)return failure("gpu_markers",active_,index_,MarkerCount);
    if(cpu_index_!=7)return failure("cpu_markers",active_,static_cast<std::int64_t>(cpu_index_),7);
    if(slots_[active_].pending)return failure("finish_pending",active_,1);
    slots_[active_].record={frame,frame_count,width,height};
    slots_[active_].pending=true;return true;
  }
};
}
