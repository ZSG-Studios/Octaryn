#include "WorldProfile.h"
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#include <share.h>
#endif

namespace octaryn::client::app {
WorldProfile::WorldProfile(const std::filesystem::path& path) {
  frame_metrics_init(&metrics_);
  const char* override_path=SDL_getenv("OCTARYN_CLIENT_PROFILE_PATH");
  const bool requested=override_path && *override_path;
  const auto output_path=requested
      ? std::filesystem::path(reinterpret_cast<const char8_t*>(override_path)) : path;
  if(requested && !output_path.parent_path().empty())
    std::filesystem::create_directories(output_path.parent_path());
#ifdef _WIN32
  file_ = _wfsopen(output_path.c_str(), L"w", _SH_DENYWR);
#else
  file_ = std::fopen(output_path.c_str(), "w");
#endif
  if(requested && !file_)throw std::runtime_error("Cannot open requested world CPU profile output");
  if (file_)
    std::fprintf(file_, "frame,time_seconds,frame_ms,average_ms,low_1pct_fps,worst_ms,session_ms,stream_mesh_ms,render_ms,columns,quads,retained_gpu_bytes,eye_x,eye_y,eye_z,source_tick,source_seconds,drawn_columns,drawn_quads,state,ui_update_ms,movement_underruns,movement_buffer_ms,movement_holding,pending_meshes,width,height,prediction_pending,prediction_ack,prediction_replays,prediction_corrections,prediction_overflows\n");
}
WorldProfile::~WorldProfile() {
  if (file_) std::fclose(file_);
}
void WorldProfile::frame(SDL_Window* window, const frame_profile_sample& sample,
                         const LocalPlayerPose& pose, const rendering::WorldRendererStats& renderer,
                         const char* state, LocalMovementStats movement) {
  ++frames_;
  latest_ = sample;
  sim_total_ += sample.sim_ms;
  world_total_ += sample.world_ms;
  render_total_ += sample.render_ms;
  ui_total_ += sample.ui_ms;
  ++report_samples_;
  const auto now = SDL_GetTicksNS();
  frame_metrics_record(&metrics_, sample.total_ms, now);
  if (metrics_.sample_count && sample.total_ms >= 5) {
    auto lowest = std::min_element(slow_frames_.begin(), slow_frames_.end(),
        [](const SlowFrame& a, const SlowFrame& b) { return a.sample.total_ms < b.sample.total_ms; });
    if (sample.total_ms > lowest->sample.total_ms) *lowest = {frames_, sample};
  }
  if (now - last_report_ < 1000000000ull) return;
  last_report_ = now;
  const auto stats = frame_metrics_snapshot_value(&metrics_, now);
  char title[256];
  std::snprintf(title, sizeof(title),
                "Octaryn | %.0f FPS | %u columns | %s | WASD mouse, F fly, Esc cursor",
                stats.current.fps, renderer.columns, state);
  SDL_SetWindowTitle(window, title);
  if (file_) {
    const double count = static_cast<double>(report_samples_);
    int width{},height{};
    SDL_GetWindowSizeInPixels(window,&width,&height);
    std::fprintf(file_, "%llu,%.3f,%.3f,%.3f,%.2f,%.3f,%.3f,%.3f,%.3f,%u,%llu,%llu,%.3f,%.3f,%.3f,%llu,%.6f,%u,%llu,%s,%.3f,%llu,%.3f,%u,%u,%d,%d,%llu,%llu,%llu,%llu,%llu\n",
                  static_cast<unsigned long long>(frames_), static_cast<double>(now) / 1e9,
                  sample.total_ms, stats.average.ms, stats.low_1pct.fps, stats.worst.ms,
                  sim_total_ / count, world_total_ / count, render_total_ / count,
                  renderer.columns, static_cast<unsigned long long>(renderer.quads),
                  static_cast<unsigned long long>(renderer.gpu_bytes),
                  pose.x, pose.y, pose.z,
                  static_cast<unsigned long long>(pose.source_tick), pose.source_seconds,
                  renderer.drawn_columns, static_cast<unsigned long long>(renderer.drawn_quads), state, ui_total_ / count,
                  static_cast<unsigned long long>(movement.underruns), movement.buffered_seconds*1000, movement.holding?1u:0u,renderer.pending_meshes,width,height,
 static_cast<unsigned long long>(movement.pending),static_cast<unsigned long long>(movement.ack),
 static_cast<unsigned long long>(movement.replays),static_cast<unsigned long long>(movement.corrections),
 static_cast<unsigned long long>(movement.overflows));
    std::fflush(file_);
  }
  sim_total_ = world_total_ = render_total_ = ui_total_ = 0;
  report_samples_ = 0;
}
frame_profile_snapshot WorldProfile::snapshot() const {
  return {latest_, frame_metrics_snapshot_value(&metrics_, SDL_GetTicksNS())};
}
void WorldProfile::restart_measurement() {
  frame_metrics_begin_measurement(&metrics_);
  sim_total_ = world_total_ = render_total_ = ui_total_ = 0;
  report_samples_ = 0;
  last_report_ = SDL_GetTicksNS();
  slow_frames_ = {};
}
void WorldProfile::report_slow_frames() const {
  auto frames = slow_frames_;
  std::sort(frames.begin(), frames.end(),
      [](const SlowFrame& a, const SlowFrame& b) { return a.sample.total_ms > b.sample.total_ms; });
  for (const auto& frame : frames) if (frame.frame) {
    const auto& s = frame.sample;
    std::printf("world_slow_frame frame=%llu total_ms=%.3f session_ms=%.3f stream_mesh_ms=%.3f render_ms=%.3f events_ms=%.3f profile_ms=%.3f ui_update_ms=%.3f other_ms=%.3f\n",
        static_cast<unsigned long long>(frame.frame), s.total_ms, s.sim_ms, s.world_ms,
        s.render_ms, s.misc_ms, s.post_submit_tail_ms, s.ui_ms,
        std::max(0.0f, s.total_ms-s.sim_ms-s.world_ms-s.render_ms-s.misc_ms-s.post_submit_tail_ms-s.ui_ms));
  }
}
}
