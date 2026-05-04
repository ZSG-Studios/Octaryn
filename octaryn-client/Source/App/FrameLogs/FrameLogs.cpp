#include "FrameLogs.h"

#include "Log.h"

#include <cinttypes>
#include <cstdio>

namespace octaryn_client_app {

void log_live_client_frame(uint64_t frame_index,
                           const client_input_debug_state &input,
                           const client_command_frame_counts &commands,
                           const camera &camera,
                           uint32_t drained_updates,
                           const std::vector<presentation_block> &blocks) {
  if (g_log == nullptr) {
    return;
  }

  if (frame_index == 1u || input.active || drained_updates != 0u ||
      frame_index % 60u == 0u) {
    std::fprintf(g_log,
                 "live_input_frame frame=%" PRIu64
                 " active=%d move=(%.3f,%.3f,%.3f) flags=%" PRIu32
                 " controller=%" PRIu32 " relative_mouse=%d\n",
                 frame_index, input.active ? 1 : 0, input.move_x, input.move_y,
                 input.move_z, input.flags, input.controller,
                 input.relative_mouse);
    std::fprintf(g_log,
                 "live_camera_frame frame=%" PRIu64
                 " active=%d mode=live_runtime x=%.3f y=%.3f z=%.3f"
                 " pitch=%.6f yaw=%.6f far=%.1f look=(%.3f,%.3f)\n",
                 frame_index, input.active ? 1 : 0, camera.position[0],
                 camera.position[1], camera.position[2], camera.pitch_radians,
                 camera.yaw_radians, camera.far_plane, input.look_pitch,
                 input.look_yaw);
    std::fprintf(g_log,
                 "live_movement_frame frame=%" PRIu64
                 " active=%d speed=%.3f move=(%.3f,%.3f,%.3f)"
                 " sprint=%d fly=1\n",
                 frame_index, input.active ? 1 : 0, input.speed, input.move_x,
                 input.move_y, input.move_z,
                 (input.flags & kInputSprintFlag) != 0u ? 1 : 0);
    std::fprintf(g_log,
                 "live_interaction_frame frame=%" PRIu64
                 " primary=%d secondary=%d command_enqueue_hook=active"
                 " commands_enqueued=%" PRIu32 " set_block=%" PRIu32
                 " place=%" PRIu32 " break=%" PRIu32 "\n",
                 frame_index, (input.flags & kInputPrimaryFlag) != 0u ? 1 : 0,
                 (input.flags & kInputSecondaryFlag) != 0u ? 1 : 0,
                 commands.enqueued, commands.set_block, commands.place_block,
                 commands.break_block);
    std::fprintf(g_log,
                 "live_presentation_frame frame=%" PRIu64
                 " blocks=%zu drained_updates=%" PRIu32 "\n",
                 frame_index, blocks.size(), drained_updates);
    std::fflush(g_log);
  }
}

void log_frame_profile(uint64_t frame_index,
                       const frame_profile_snapshot &profile,
                       uint8_t debug_overlay_enabled) {
  if (g_log == nullptr) {
    return;
  }

  if (frame_index <= 5u || frame_index % 60u == 0u ||
      debug_overlay_enabled != 0u) {
    const frame_profile_sample &sample = profile.sample;
    std::fprintf(
        g_log,
        "live_frame_profile frame=%" PRIu64
        " fps=%.1f avg_fps=%.1f low_1_fps=%.1f low_0_1_fps=%.1f"
        " low_x5_fps=%.1f low_x10_fps=%.1f worst_fps=%.1f"
        " frame_ms=%.3f avg_ms=%.3f low_1_ms=%.3f low_0_1_ms=%.3f"
        " low_x5_ms=%.3f low_x10_ms=%.3f worst_ms=%.3f"
        " sim_ms=%.3f misc_ms=%.3f world_ms=%.3f render_ms=%.3f"
        " setup_ms=%.3f other_ms=%.3f gbuffer_ms=%.3f sky_ms=%.3f"
        " opaque_ms=%.3f sprite_ms=%.3f post_ms=%.3f composite_ms=%.3f"
        " depth_ms=%.3f forward_ms=%.3f ui_ms=%.3f imgui_ms=%.3f"
        " blit_ms=%.3f submit_ms=%.3f acquire_ms=%.3f command_ms=%.3f"
        " swap_wait_ms=%.3f untracked_ms=%.3f warmup=%u samples=%" PRIu64 "\n",
        frame_index, profile.metrics.current.fps, profile.metrics.average.fps,
        profile.metrics.low_1pct.fps, profile.metrics.low_0_1pct.fps,
        profile.metrics.confirmed_low_5.fps,
        profile.metrics.confirmed_low_10.fps, profile.metrics.worst.fps,
        sample.total_ms, profile.metrics.average.ms,
        profile.metrics.low_1pct.ms, profile.metrics.low_0_1pct.ms,
        profile.metrics.confirmed_low_5.ms, profile.metrics.confirmed_low_10.ms,
        profile.metrics.worst.ms, sample.sim_ms, sample.misc_ms,
        sample.world_ms, sample.render_ms, sample.render_setup_ms,
        sample.render_other_ms, sample.gbuffer_ms, sample.gbuffer_sky_ms,
        sample.gbuffer_opaque_ms, sample.gbuffer_sprite_ms, sample.post_ms,
        sample.composite_ms, sample.depth_ms, sample.forward_ms, sample.ui_ms,
        sample.imgui_ms, sample.swapchain_blit_ms, sample.render_submit_ms,
        sample.frame_acquire_ms, sample.command_acquire_ms,
        sample.swapchain_wait_ms, sample.untracked_ms,
        static_cast<unsigned>(profile.metrics.warmup_complete),
        profile.metrics.sample_count);
    std::fflush(g_log);
  }
}

} // namespace octaryn_client_app
