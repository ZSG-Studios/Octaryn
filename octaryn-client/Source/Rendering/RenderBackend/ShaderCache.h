#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <filesystem>
namespace octaryn::client::rendering {
// Stores opaque SDK keys/blobs; Slang/RHI own dependency and PSO validity.
Slang::ComPtr<rhi::IPersistentCache> create_shader_cache(const std::filesystem::path& directory);
struct ShaderCaches {
  Slang::ComPtr<rhi::IPersistentCache> shaders,pipelines;
};
ShaderCaches configure_shader_caches(rhi::DeviceDesc&);
}
