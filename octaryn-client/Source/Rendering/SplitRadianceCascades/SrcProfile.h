#pragma once
#include <slang-rhi.h>
#include <cstdint>
namespace octaryn::client::rendering {
struct SplitRadianceCascades;
struct WorldRenderer;
// Env-gated (OCTARYN_SRC_PROFILE) per-frame counter readback for hang/cost
// attribution. Writes one JSON line per frame to OCTARYN_SRC_PROFILE_PATH or
// stderr. Reads happen after the previous-frame fence wait in world_src_update,
// so the dump describes frame N-2 while frame N is being encoded.
bool src_profile_enabled();
bool src_profile_poll(SplitRadianceCascades&,rhi::IDevice*,std::uint64_t frame);
}
