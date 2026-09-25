#pragma once
#include <cstdint>
#include <cstdio>

namespace octaryn::client::rendering {
// Failure-only logging; keep the failing RHI result and the last completed step.
struct RayPrepareDiagnostics {
  const char* phase;
  const char* last{"begin"};
  std::uint64_t frame{},generation{},bytes{},observed{},signal{};
  std::int32_t x{},z{};
  std::uint32_t faces{};

  bool check(const char* step,std::int32_t result) {
    if(result>=0) {last=step;return true;}
    std::fprintf(stderr,"ray_prepare_failed phase=%s step=%s last_success=%s result=0x%08x "
      "frame=%llu generation=%llu column=%d,%d faces=%u requested_bytes=%llu fence=%llu/%llu\n",
      phase,step,last,static_cast<unsigned>(result),static_cast<unsigned long long>(frame),
      static_cast<unsigned long long>(generation),x,z,faces,static_cast<unsigned long long>(bytes),
      static_cast<unsigned long long>(observed),static_cast<unsigned long long>(signal));
    return false;
  }
  bool require(const char* step,bool ok) {return check(step,ok?0:-1);}
};
}
