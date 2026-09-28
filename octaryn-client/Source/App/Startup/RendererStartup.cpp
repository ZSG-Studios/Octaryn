#include "RendererStartup.h"
#include "StartupWork.h"
#include "WorldRenderer.h"
#include "LoadingScreen.h"
#include "RuntimeControls.h"
#include "octaryn_native_schedule_runtime.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

namespace octaryn::client::app {
namespace {
namespace graphics = octaryn::client::rendering;
struct Startup {
  SDL_Window* window{};
  const runtime_controls* settings{};
  StartupWork work;
  std::exception_ptr failure;
  graphics::WorldRenderer* renderer{};
  Uint64 elapsed_ns{};

  void prepare_saved_settings() {
    const auto started=SDL_GetTicksNS();
    const auto& saved=*settings;
    StartupWork::progress("saved lighting settings",&work);
    graphics::open_world_renderer_set_reflection_quality(renderer,saved.reflection_quality);
    graphics::open_world_renderer_set_shadow_quality(renderer,saved.shadow_quality);
    graphics::open_world_renderer_set_trace_ranges(renderer,float(saved.shadow_distance),
        float(saved.reflection_distance));
    const auto lighting_ms=double(SDL_GetTicksNS()-started)/1e6;
    StartupWork::progress("saved temporal presentation",&work);
    graphics::WorldSceneSettings scene;
    scene.ray_tracing=saved.ray_tracing_enabled!=0;
    scene.upscaler_mode=saved.upscaler_mode;
    scene.fsr_sharpening=saved.fsr_sharpening!=0;
    scene.fsr_sharpness=saved.fsr_sharpness;
    scene.fsr_render_scale=saved.fsr_render_scale;
    scene.fsr_dynamic_resolution=saved.fsr_dynamic_resolution!=0;
    scene.fsr_min_scale=saved.fsr_min_scale;
    scene.fsr_max_scale=saved.fsr_max_scale;
    scene.fsr_target_fps=saved.fsr_target_fps;
    graphics::open_world_renderer_set_scene(renderer,scene);
    if(!graphics::open_world_renderer_prepare_temporal(renderer))
      throw std::runtime_error("Saved temporal presentation preparation failed");
    StartupWork::progress("module item presentation",&work);
    if(!graphics::open_world_renderer_prepare_items(renderer))
      throw std::runtime_error("Module item presentation preparation failed");
    std::printf("client_boot_settings lighting_ms=%.1f temporal_ms=%.1f result=ready\n",
        lighting_ms,double(SDL_GetTicksNS()-started)/1e6-lighting_ms);
    StartupWork::progress("graphics ready",&work);
  }

  static int execute(void* user) noexcept {
    auto& state=*static_cast<Startup*>(user);
    const auto started=SDL_GetTicksNS();
    try {
      StartupWork::progress("graphics device",&state.work);
      state.renderer=graphics::open_world_renderer_create(state.window,StartupWork::progress,&state.work,StartupWork::main_thread);
      // Creation has completed all main-thread handoffs. Keep exclusive RHI
      // ownership while allocating the saved world settings before menu/play.
      if(state.renderer)state.prepare_saved_settings();
    } catch(const StartupWork::Cancelled&) {
    } catch(...) {state.failure=std::current_exception();}
    state.elapsed_ns=SDL_GetTicksNS()-started;
    return 0;
  }
};
}

graphics::WorldRenderer* start_renderer(SDL_Window* window, bool& running,
    const runtime_controls& settings) {
  using Runtime=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_destroy)>;
  using Task=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_task_destroy)>;
  Runtime runtime(octaryn_native_schedule_runtime_create(SDL_GetNumLogicalCPUCores(),2),
      octaryn_native_schedule_runtime_destroy);
  if(!runtime)throw std::runtime_error("Cannot create renderer startup scheduler");
  Startup state;
  state.window=window;
  state.settings=&settings;
  octaryn_native_schedule_runtime_job job{};
  job.job_id="renderer_startup";
  job.execute=Startup::execute;
  job.context=&state;
  Task task(octaryn_native_schedule_runtime_submit_worker(runtime.get(),&job,1),
      octaryn_native_schedule_runtime_task_destroy);
  if(!task)throw std::runtime_error("Cannot schedule renderer startup");
  const auto started=SDL_GetTicksNS();
  auto previous=started;
  Uint64 maximum_gap{};
  unsigned pumps{};
  std::string last_stage;
  while(!octaryn_native_schedule_runtime_task_ready(task.get())) {
    const auto now=SDL_GetTicksNS();
    if(now-previous>maximum_gap)maximum_gap=now-previous;
    previous=now;
    ++pumps;
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
          event.window.windowID==SDL_GetWindowID(window))) {
        running=false;
        state.work.cancel();
        SDL_SetWindowTitle(window,"Octaryn | Finishing current graphics operation before closing");
      }
    }
    const auto stage=state.work.pump();
    if(running && stage!=last_stage) {
      pump_boot_stage(window,stage.c_str());
      last_stage=stage;
    }
    SDL_Delay(8);
  }
  octaryn_native_schedule_runtime_report report{};
  const auto final_gap=SDL_GetTicksNS()-previous;
  if(final_gap>maximum_gap)maximum_gap=final_gap;
  const int result=octaryn_native_schedule_runtime_task_result(task.get(),&report);
  task.reset();
  std::printf("client_boot responsiveness=1 worker_jobs=%zu event_pumps=%u max_event_gap_ms=%.2f worker_elapsed_ms=%.0f elapsed_ms=%.0f result=%s\n",
      report.worker_jobs,pumps,double(maximum_gap)/1e6,double(state.elapsed_ns)/1e6,
      double(SDL_GetTicksNS()-started)/1e6,!running?"cancelled":state.failure || result!=0 || !state.renderer?"failed":"ready");
  if(!running || state.failure || result!=0) {
    graphics::open_world_renderer_destroy(state.renderer);
    if(state.failure)std::rethrow_exception(state.failure);
    if(result!=0)throw std::runtime_error("Renderer startup job failed");
    return nullptr;
  }
  return state.renderer;
}
}
