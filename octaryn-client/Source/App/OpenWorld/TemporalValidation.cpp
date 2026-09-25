#include "TemporalValidation.h"
#include "WorldRenderer.h"
#include "RuntimeControls.h"
#include <SDL3/SDL.h>
#include <RmlUi/Core/Context.h>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace octaryn::client::app {
namespace {
struct Phase {unsigned mode;int width,height;};
constexpr Phase phases[]={{0,1280,720},{1,1280,720},{2,1280,720},{3,1280,720},
    {4,1280,720},{5,1280,720},{2,1280,720},{2,960,540},{2,1280,720}};
constexpr unsigned phase_count=sizeof(phases)/sizeof(phases[0]);
constexpr unsigned required_frames=12;
unsigned render_extent(int pixels,unsigned mode) {
  constexpr double ratios[]={1,1,1.5,1.7,2,3};
  return static_cast<unsigned>(std::lround(double(pixels)/ratios[mode]));
}
}

void TemporalValidation::begin_frame(SDL_Window* window,runtime_controls& controls,
    const rendering::WorldRendererStats& prior,double seconds) {
  if(complete_)return;
  if(started_<0)started_=seconds;
  if(resident_started_<0 && seconds-started_>90)
    throw std::runtime_error("Temporal validation did not reach full residency within90 seconds");
  if(requested_) {
    if(resident_started_>=0 && seconds-phase_started_>45)
      throw std::runtime_error("Temporal validation mode/resize/capture phase stalled");
    return;
  }
  if(const char* value=SDL_getenv("OCTARYN_CLIENT_UPSCALER");value&&*value)
    throw std::runtime_error("Temporal validation cannot use an environment upscaler override");
  const auto& phase=phases[phase_];
  if(!SDL_SetWindowFullscreen(window,false)||!SDL_SetWindowSize(window,phase.width,phase.height))
    throw std::runtime_error(SDL_GetError());
  controls.upscaler_mode=static_cast<std::uint8_t>(phase.mode);
  previous_frame_=prior.frames;previous_resets_=prior.temporal_resets;
  resident_frames_=0;phase_resets_=0;phase_started_=seconds;requested_=true;
}

void TemporalValidation::camera(rendering::WorldCamera& camera) const {
  // Successful-frame motion is deterministic and independent of CPU frame pacing.
  camera.yaw+=static_cast<float>(total_frames_)*.0015f;
}

bool TemporalValidation::capture_ready() const {
  return phase_+1==phase_count && resident_frames_>=required_frames && !complete_;
}

void TemporalValidation::frame_rendered(const rendering::WorldRendererStats& stats,Rml::Context* ui,
    SDL_Window* window,bool resident,bool rendered,bool captured,double seconds) {
  if(complete_||!requested_||!rendered||stats.frames==previous_frame_)return;
  if(stats.frames<previous_frame_)throw std::runtime_error("Temporal validation renderer clock reversed");
  previous_frame_=stats.frames;
  if(!resident) {resident_frames_=0;return;}
  if(resident_started_<0) {resident_started_=seconds;phase_started_=seconds;}
  int logical_width{},logical_height{},width{},height{};
  if(!SDL_GetWindowSize(window,&logical_width,&logical_height)||!SDL_GetWindowSizeInPixels(window,&width,&height))
    throw std::runtime_error(SDL_GetError());
  const auto& phase=phases[phase_];
  // SDL window sizes are logical points on high-density displays. Compare renderer
  // and Rml against native pixels, while independently verifying requested window size.
  const bool matched=logical_width==phase.width&&logical_height==phase.height&&width>0&&height>0&&
      stats.upscaler_mode==phase.mode&&stats.display_width==static_cast<unsigned>(width)&&
      stats.display_height==static_cast<unsigned>(height)&&stats.render_width==render_extent(width,phase.mode)&&
      stats.render_height==render_extent(height,phase.mode);
  if(!matched) {
    resident_frames_=0;
    if(seconds-phase_started_>5)throw std::runtime_error("Temporal validation actual mode/render/output dimensions mismatch");
    return;
  }
  if(!ui||ui->GetDimensions().x!=width||ui->GetDimensions().y!=height)
    throw std::runtime_error("Temporal validation RmlUi is not at native output dimensions");
  if(phase.mode && stats.temporal_resets<=previous_resets_)
    throw std::runtime_error("Temporal mode/resize transition did not reset submitted history");
  if(resident_frames_ && stats.temporal_resets!=phase_resets_)
    throw std::runtime_error("Temporal history unexpectedly reset within a stable phase");
  phase_resets_=stats.temporal_resets;
  ++resident_frames_;++total_frames_;
  if(resident_frames_<required_frames)return;
  if(phase_+1==phase_count && !captured)return;
  std::printf("temporal_validation phase=%u mode=%u window=%dx%d render=%ux%u output=%ux%u ui=%dx%d resets=%llu successful_frames=%u\n",
      phase_,phase.mode,logical_width,logical_height,stats.render_width,stats.render_height,
      stats.display_width,stats.display_height,width,height,
      static_cast<unsigned long long>(stats.temporal_resets),resident_frames_);
  if(++phase_==phase_count) {
    complete_=true;
    std::printf("temporal_validation=passed modes=6 phases=%u successful_frames=%u final_capture=1 os_events_injected=0\n",
        phase_count,total_frames_);
  } else requested_=false;
  std::fflush(stdout);
}
}
