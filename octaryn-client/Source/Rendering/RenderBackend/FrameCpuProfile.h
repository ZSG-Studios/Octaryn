#pragma once
#include "../../Diagnostics/AsyncProfileStream.h"
#include "FrameFenceRecord.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <iomanip>
namespace octaryn::client::rendering {
// One renderer owner, fixed records, bounded asynchronous bytes; no worker holds GPU objects.
class FrameCpuProfile {
  struct Interval {const char* stage{};std::uint64_t start{},end{};std::int64_t cpu{-1};};
  std::array<Interval,128> intervals_{};
  std::array<FrameFenceRecord,4> fences_{};
  diagnostics::AsyncProfileStream output_;
  std::size_t count_{},fence_count_{};
  std::uint64_t sequence_{},frame_{},thread_{},start_{};
  const char* scope_{};
  bool enabled_{},failed_{},closed_{};
  void fail(const char* reason) {
    if(!failed_)std::fprintf(stderr,"profile_writer_failed capture_invalid=1 owner=frame_cpu reason=%s\n",reason);
    failed_=true;
  }
  void prefix(const char* kind,const char* stage,std::uint64_t start,std::uint64_t end,std::int64_t cpu) {
    output_<<"1,"<<sequence_++<<','<<kind<<','<<frame_<<','<<scope_<<','<<stage<<','<<thread_<<','
        <<start<<','<<end<<','<<double(end-start)/1e6<<','<<cpu;
  }
public:
  bool initialize() {
    const auto* option=std::getenv("OCTARYN_CLIENT_FRAME_CPU_TRACE");
    if(!option || !*option || !std::strcmp(option,"0"))return true;
    if(std::strcmp(option,"1")) {fail("invalid_option");return false;}
    const auto* explicit_path=std::getenv("OCTARYN_CLIENT_FRAME_CPU_TRACE_PATH");
    const auto* gpu_path=std::getenv("OCTARYN_CLIENT_GPU_PROFILE_PATH");
    std::string path=explicit_path?explicit_path:"";
    if(path.empty() && gpu_path && *gpu_path)path=std::string(gpu_path)+".retirement.csv";
    if(path.empty()) {fail("missing_path");return false;}
    const std::filesystem::path target(reinterpret_cast<const char8_t*>(path.c_str()));
    if(target.has_parent_path())std::filesystem::create_directories(target.parent_path());
    output_.open(target);
    output_<<std::setprecision(9);
    output_<<"schema_version,sequence,record,renderer_frame,scope,stage,thread_id,start_ns,end_ns,wall_ms,thread_cpu_ns,slot,fence_value,source_frame,completed_before,completed_after,wait_called,success,result,interval_count\n";
    enabled_=bool(output_);if(!enabled_)fail("open");
    std::printf("frame_cpu_profile enabled=%u schema=1 bounded_intervals=128 bounded_fences=4 queue_bytes=1048576 clock=steady_clock thread_cpu=GetThreadTimes_when_available pending_frame_on_termination=unknown\n",unsigned(enabled_));
    return enabled_;
  }
  bool enabled() const {return enabled_;}
  bool healthy() const {return !failed_ && (!enabled_ || bool(output_));}
  void begin(std::uint64_t frame,std::uint64_t thread,std::uint64_t start,const char* scope) {
    frame_=frame;thread_=thread;start_=start;scope_=scope;count_=fence_count_=0;
  }
  void interval(const char* stage,std::uint64_t start,std::uint64_t end,std::int64_t cpu) {
    if(end<start || count_==intervals_.size()) {fail("interval_capacity_or_clock");return;}
    intervals_[count_++]={stage,start,end,cpu};
  }
  void fence(const FrameFenceRecord& record) {
    if(fence_count_==fences_.size()) {fail("fence_capacity");return;}
    fences_[fence_count_++]=record;
  }
  void finish(std::uint64_t end,const char* status) {
    if(!enabled_)return;
    for(std::size_t i=0;i<count_;++i) {
      const auto& r=intervals_[i];prefix("interval",r.stage,r.start,r.end,r.cpu);
      output_<<",-1,0,"<<UINT64_MAX<<','<<UINT64_MAX<<','<<UINT64_MAX<<",0,1,0,0\n";
    }
    for(std::size_t i=0;i<fence_count_;++i) {
      const auto& r=fences_[i];
      const char* names[]{"fence_precheck","fence_wait_call","fence_postcheck"};
      for(unsigned part=0;part<3;++part) {
        prefix("fence",names[part],r.times[part],r.times[part+1],-1);
        output_<<','<<r.slot<<','<<r.value<<','<<r.source_frame<<','<<r.before<<','<<r.after<<','
            <<unsigned(r.waited)<<','<<unsigned(r.success)<<','<<r.result<<",0\n";
      }
    }
    prefix("frame",status,start_,end,-1);
    output_<<",-1,0,"<<UINT64_MAX<<','<<UINT64_MAX<<','<<UINT64_MAX<<",0,"
        <<unsigned(!failed_ && !std::strcmp(status,"complete"))<<",0,"<<count_<<'\n';
    // This only queues bytes. Disk writes and flushes are confined to the worker.
    output_.flush();if(!output_)fail("write_or_queue");
  }
  bool close() {
    if(closed_)return healthy();closed_=true;
    if(enabled_ && !output_.close())fail("shutdown");
    return healthy();
  }
  ~FrameCpuProfile(){close();}
};
}
