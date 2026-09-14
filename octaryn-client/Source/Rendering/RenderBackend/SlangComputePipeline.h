#pragma once

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-com-ptr.h>
#include <slang-gfx.h>

namespace octaryn::client::rendering {

bool create_slang_compute_pipeline(gfx::IDevice *device, const char *source_path,
                                  Slang::ComPtr<gfx::IPipelineState> &pipeline);

} // namespace octaryn::client::rendering
#endif
