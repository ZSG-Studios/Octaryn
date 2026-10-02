#pragma once
#include <SDL3/SDL_timer.h>
#include <array>
#include <cstdint>
#include <cstdio>

namespace octaryn::client::rendering {
enum class RmlWork : unsigned { Geometry, TextureLoad, TextureUpload, Draw, Surface, Count };
struct RmlProfile {
  bool enabled{};
  std::array<std::uint64_t,unsigned(RmlWork::Count)> nanoseconds{},calls{};
  std::uint64_t geometry_bytes{},texture_bytes{};
  void reset() {nanoseconds.fill(0);calls.fill(0);geometry_bytes=texture_bytes=0;}
  void report(std::uint64_t frame,std::uint64_t elapsed) const {
    if(!enabled || (elapsed<25000000 && frame!=1 && frame%60!=0))return;
    std::printf("rml_cpu frame=%llu render_ms=%.3f geometry_ms=%.3f geometry_calls=%llu geometry_bytes=%llu "
        "texture_load_ms=%.3f texture_load_calls=%llu texture_upload_ms=%.3f texture_upload_calls=%llu texture_bytes=%llu "
        "draw_ms=%.3f draw_calls=%llu surface_ms=%.3f surface_calls=%llu\n",
        static_cast<unsigned long long>(frame),double(elapsed)/1e6,
        double(nanoseconds[0])/1e6,static_cast<unsigned long long>(calls[0]),static_cast<unsigned long long>(geometry_bytes),
        double(nanoseconds[1])/1e6,static_cast<unsigned long long>(calls[1]),
        double(nanoseconds[2])/1e6,static_cast<unsigned long long>(calls[2]),static_cast<unsigned long long>(texture_bytes),
        double(nanoseconds[3])/1e6,static_cast<unsigned long long>(calls[3]),
        double(nanoseconds[4])/1e6,static_cast<unsigned long long>(calls[4]));
  }
};
struct RmlScopeTimer {
  RmlProfile& profile;
  unsigned slot;
  std::uint64_t begin;
  RmlScopeTimer(RmlProfile& value,RmlWork work):profile(value),slot(unsigned(work)),
      begin(value.enabled?SDL_GetTicksNS():0) {}
  ~RmlScopeTimer() {
    if(profile.enabled){profile.nanoseconds[slot]+=SDL_GetTicksNS()-begin;++profile.calls[slot];}
  }
};
}
