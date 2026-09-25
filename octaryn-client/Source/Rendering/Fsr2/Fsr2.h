#pragma once
#include <slang-rhi.h>
#include <cstdint>

namespace octaryn::client::rendering {
struct Fsr2Context;
struct Fsr2CreateDesc {
    // Half-size luminance requires both axes >=2 and the larger axis >=64.
    uint32_t max_render_width{}, max_render_height{}, display_width{}, display_height{};
    bool hdr{true}, depth_inverted{}, depth_infinite{}, dynamic_resolution{};
};
struct Fsr2Dispatch {
    rhi::ITexture* color{};
    rhi::ITexture* depth{};
    rhi::ITexture* motion_vectors{};
    rhi::ITexture* reactive{};
    rhi::ITexture* transparency{};
    rhi::ITexture* exposure{};
    rhi::ITexture* output{};
    uint32_t render_width{}, render_height{};
    float jitter_x{}, jitter_y{}, motion_scale_x{1}, motion_scale_y{1};
    float delta_ms{}, near_plane{0.1f}, far_plane{8192}, vertical_fov{};
    float pre_exposure{1}, sharpness{};
    bool reset{}, sharpen{};
    float reprojection[16]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
};
struct Fsr2Jitter { float x{}, y{}; };
// Caller submits dispatches chronologically on one queue. The encoder records
// inter-pass barriers; output/history must not be concurrently used on another queue.
// Destroy/recreate only after the caller's last submitted frame has completed.
Fsr2Context* create_fsr2(rhi::IDevice*, const Fsr2CreateDesc&);
void destroy_fsr2(Fsr2Context*);
bool dispatch_fsr2(Fsr2Context*, rhi::ICommandEncoder*, const Fsr2Dispatch&);
const char* fsr2_error(const Fsr2Context*);
uint64_t fsr2_gpu_bytes(const Fsr2Context*);
Fsr2Jitter fsr2_jitter(uint32_t frame, uint32_t render_width, uint32_t display_width);
}
