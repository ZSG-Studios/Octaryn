#pragma once
#include "../../Diagnostics/AsyncProfileStream.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace octaryn::client::app {
// Begin records are queued immediately, so a blocked post-render call is visible.
// The existing bounded writer fails admission on saturation; it never drops rows.
class PostRenderTrace {
  diagnostics::AsyncProfileStream output_;
  std::uint64_t frame_{},ready_{},sequence_{};
  unsigned records_{};
  bool enabled_{};
public:
  PostRenderTrace() {
    const auto* option=std::getenv("OCTARYN_CLIENT_FRAME_CPU_TRACE");
    if(!option || !*option || !std::strcmp(option,"0"))return;
    const auto* path=std::getenv("OCTARYN_CLIENT_FRAME_CPU_TRACE_PATH");
    const auto* gpu=std::getenv("OCTARYN_CLIENT_GPU_PROFILE_PATH");
    std::string target=path?path:"";
    if(target.empty() && gpu && *gpu)target=std::string(gpu)+".retirement.csv";
    if(std::strcmp(option,"1") || target.empty())throw std::runtime_error("Invalid post-render trace option/path");
    target+=".post-render.csv";
    output_.open(std::filesystem::path(reinterpret_cast<const char8_t*>(target.c_str())));
    if(!output_)throw std::runtime_error("Cannot open bounded post-render trace");
    enabled_=true;output_<<"schema_version,sequence,renderer_frame,ready_frame,stage,steady_ns,requested_wait_ns,actual_wait_ns,wait_calls,last_timeout_ms,wait_result\n";
    output_.flush();
  }
  void begin(std::uint64_t frame,std::uint64_t ready) {frame_=frame;ready_=ready;records_=0;stage("renderer_return");}
  void stage(const char* name,std::uint64_t requested=0,std::uint64_t actual=0,
      unsigned calls=0,unsigned timeout=0,unsigned result=0) {
    if(!enabled_)return;
    if(++records_>32)throw std::runtime_error("Post-render trace stage bound exceeded");
    const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    output_<<"1,"<<sequence_++<<','<<frame_<<','<<ready_<<','<<name<<','<<now<<','
        <<requested<<','<<actual<<','<<calls<<','<<timeout<<','<<result<<'\n';output_.flush();
    if(!output_)throw std::runtime_error("profile_writer_failed capture_invalid=1 owner=post_render");
  }
};
}
