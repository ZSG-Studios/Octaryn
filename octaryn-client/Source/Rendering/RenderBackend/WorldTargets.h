#pragma once
#include "WorldHdr.h"

namespace octaryn::client::rendering {
struct WorldTargets {
  bool initialized{};
  WorldHdr hdr;
  Slang::ComPtr<rhi::ITexture> depth,color;
  Slang::ComPtr<rhi::ITextureView> depth_view,color_view;
};
}
