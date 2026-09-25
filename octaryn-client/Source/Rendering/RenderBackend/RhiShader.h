#pragma once

#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <cstdint>

namespace octaryn::client::rendering {

bool create_rhi_program(rhi::IDevice* device, const char* path,
                        const char* const* entries, uint32_t count,
                        Slang::ComPtr<rhi::IShaderProgram>& program);
bool create_rhi_compute_pipeline(rhi::IDevice* device, const char* path,
                                 const char* entry,
                                 Slang::ComPtr<rhi::IComputePipeline>& pipeline);

} // namespace octaryn::client::rendering
