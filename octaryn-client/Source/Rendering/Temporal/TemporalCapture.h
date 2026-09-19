#pragma once
#include "WorldTemporal.h"
namespace octaryn::client::rendering {
bool capture_temporal(const WorldTemporal&,rhi::IDevice*,rhi::ITexture* scene,
    rhi::ITexture* depth,unsigned slot,const char* capture_path);
bool capture_temporal_observation(const WorldTemporal&,std::uint64_t frame,unsigned slot,
    double readback_and_write_ms,const char* capture_path);
}
