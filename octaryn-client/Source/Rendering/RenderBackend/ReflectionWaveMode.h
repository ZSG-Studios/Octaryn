#pragma once
#include <slang-rhi.h>
#include <cstring>
#include <cstdio>
namespace octaryn::client::rendering {
struct ReflectionWaveMode {
  unsigned requested{};
  bool telemetry{},reported{};
  const char* macro() const {return requested==32?"32":requested==64?"64":"0";}
  bool configure(const char* option,rhi::DeviceType backend,bool collecting) {
    if(option && std::strcmp(option,"0") && std::strcmp(option,"32") && std::strcmp(option,"64"))return false;
    requested=option && !std::strcmp(option,"32")?32u:option && !std::strcmp(option,"64")?64u:0u;
    telemetry=collecting;
    return !requested || backend==rhi::DeviceType::D3D12;
  }
  bool supported(bool wave_ops,unsigned minimum,unsigned maximum) const {
    return (!telemetry || wave_ops) && (!requested || (wave_ops && minimum && minimum<=requested && requested<=maximum));
  }
  bool path(bool map_only,bool temporal,bool queued,bool ray_effects,bool ray_query,float range) const {
    return !requested || (map_only && temporal && !queued && ray_effects && ray_query && range>0);
  }
  void report(unsigned minimum,unsigned maximum) const {
    std::printf("reflection_wave_mode requested=%u forced=%u telemetry=%u device_min=%u device_max=%u actual_observed=unknown frozen=device scope=fused_map_temporal\n",
      requested,requested,unsigned(telemetry),minimum,maximum);
  }
};
}
