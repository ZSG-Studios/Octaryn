#include "WorldProfile.h"
#include "LocalSession.h"
#include <algorithm>
#include <stdexcept>

namespace octaryn::client::app {
WorldProfile::WorldProfile(const std::filesystem::path& path) {
  frame_metrics_init(&metrics_);
  const char* override_path=SDL_getenv("OCTARYN_CLIENT_PROFILE_PATH");
  const bool requested=override_path && *override_path;
  const auto output_path=requested
      ? std::filesystem::path(reinterpret_cast<const char8_t*>(override_path)) : path;
  if(requested && !output_path.parent_path().empty())
    std::filesystem::create_directories(output_path.parent_path());
  file_.open(output_path);
  if(requested && !file_)throw std::runtime_error("Cannot open requested world CPU profile output");
  if (file_)file_<<"frame,time_seconds,frame_ms,average_ms,low_1pct_fps,worst_ms,sim_ms,render_ms,map_primitives,retained_gpu_bytes,eye_x,eye_y,eye_z,source_seconds,state,ui_update_ms,width,height\n";
}
WorldProfile::~WorldProfile() {
  if(!file_.close())std::fputs("profile_writer_failed capture_invalid=1 owner=world_summary_shutdown\n",stderr);
}
void WorldProfile::frame(SDL_Window* window, const frame_profile_sample& sample,
                         const LocalPlayerPose& player, const rendering::WorldRendererStats& renderer,
                         const char* state) {
  ++frames_;
  latest_ = sample;
  sim_total_ += sample.sim_ms;
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
                "Octaryn | %.0f FPS | %u map prims | %s | WASD mouse, F fly, Esc cursor",
                stats.current.fps, renderer.map_primitives, state);
  SDL_SetWindowTitle(window, title);
  if (file_) {
    const double count = static_cast<double>(report_samples_);
    int width{},height{};
    SDL_GetWindowSizeInPixels(window,&width,&height);
    char record[1024];
    const int size=std::snprintf(record,sizeof(record),"%llu,%.3f,%.3f,%.3f,%.2f,%.3f,%.3f,%.3f,%u,%llu,%.3f,%.3f,%.3f,%.6f,%s,%.3f,%d,%d\n",
                  static_cast<unsigned long long>(frames_), static_cast<double>(now) / 1e9,
                  sample.total_ms, stats.average.ms, stats.low_1pct.fps, stats.worst.ms,
                  sim_total_ / count, render_total_ / count,
                  renderer.map_primitives,
                  static_cast<unsigned long long>(renderer.gpu_bytes),
                  player.x, player.y, player.z, player.source_seconds, state, ui_total_ / count,
                  width,height);
    if(size<0 || static_cast<std::size_t>(size)>=sizeof(record))
      throw std::runtime_error("profile_writer_failed capture_invalid=1 owner=world_summary_record");
    file_.write(record,size);file_.flush();
  }
  sim_total_ = render_total_ = ui_total_ = 0;
  report_samples_ = 0;
}
frame_profile_snapshot WorldProfile::snapshot() const {
  return {latest_, frame_metrics_snapshot_value(&metrics_, SDL_GetTicksNS())};
}
void WorldProfile::restart_measurement() {
  frame_metrics_begin_measurement(&metrics_);
  sim_total_ = render_total_ = ui_total_ = 0;
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
    std::printf("world_slow_frame frame=%llu total_ms=%.3f sim_ms=%.3f render_ms=%.3f events_ms=%.3f profile_ms=%.3f ui_update_ms=%.3f cap_sleep_ms=%.3f other_ms=%.3f\n",
        static_cast<unsigned long long>(frame.frame), s.total_ms, s.sim_ms,
        s.render_ms, s.misc_ms, s.post_submit_tail_ms, s.ui_ms, s.fps_cap_sleep_ms,
        std::max(0.0f, s.total_ms-s.sim_ms-s.render_ms-s.misc_ms-s.post_submit_tail_ms-s.ui_ms-s.fps_cap_sleep_ms));
  }
}
}
