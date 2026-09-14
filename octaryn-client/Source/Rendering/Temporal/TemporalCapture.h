#pragma once
#include "WorldTemporal.h"
namespace octaryn::client::rendering {
bool capture_temporal(const WorldTemporal&,rhi::IDevice*,rhi::ITexture* scene,
    rhi::ITexture* depth,unsigned slot,const char* capture_path);
}
