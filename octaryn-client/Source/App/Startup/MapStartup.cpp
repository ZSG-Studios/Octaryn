#include "MapStartup.h"
#include "StartupWork.h"
#include "WorldRenderer.h"
#include "LoadingScreen.h"
#include "LocalSession.h"
#include "Prediction.h"
#include "MapManifest.h"
#include "octaryn_native_schedule_runtime.h"

#include <SDL3/SDL.h>
#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>

namespace octaryn::client::app {
namespace {
namespace graphics = octaryn::client::rendering;
struct MapStartup {
  StartupWork work;
  graphics::WorldRenderer* renderer{};
  const MapManifest* manifest{};
  local_session::MeshCollisionSoup* collision_out{};
  std::exception_ptr failure;
  bool loaded{};
  double elapsed_ms{};

  static int execute(void* user) noexcept {
    auto& state=*static_cast<MapStartup*>(user);
    const auto started=std::chrono::steady_clock::now();
    try {
      StartupWork::progress("map assets",&state.work);
      const auto path=(state.manifest->tiled?state.manifest->manifest:state.manifest->glb).generic_u8string();
      state.loaded=state.manifest->tiled
          ? graphics::open_world_renderer_load_tiles(state.renderer,reinterpret_cast<const char*>(path.c_str()))
          : graphics::open_world_renderer_load_map(state.renderer,reinterpret_cast<const char*>(path.c_str()));
      if(state.loaded && state.manifest->tiled) {
        graphics::WorldCamera anchor{};
        anchor.x=state.manifest->spawn_x;anchor.y=state.manifest->spawn_y;anchor.z=state.manifest->spawn_z;
        graphics::open_world_renderer_set_tile_anchor(state.renderer,anchor);
        while(!graphics::open_world_renderer_tile_collision_ready(state.renderer,anchor.x,anchor.y,anchor.z)) {
          StartupWork::progress("spawn tile residency",&state.work);
          if(std::chrono::steady_clock::now()-started>std::chrono::seconds(60))
            throw std::runtime_error("Spawn tile residency timed out");
          if(!graphics::open_world_renderer_prepare_tiles(state.renderer,anchor))
            throw std::runtime_error("Spawn tile preparation failed");
          SDL_Delay(1);
        }
        if(state.collision_out) *state.collision_out=local_session::MeshCollisionSoup(
            graphics::open_world_renderer_tile_collision(state.renderer),
            [renderer=state.renderer](float x,float y,float z) {
              return graphics::open_world_renderer_tile_collision_ready(renderer,x,y,z);
            });
      }
      if(state.loaded) {
        StartupWork::progress("temporal presentation",&state.work);
        const auto temporal_started=std::chrono::steady_clock::now();
        state.loaded=graphics::open_world_renderer_prepare_temporal(state.renderer);
        std::printf("map_boot_temporal ms=%.1f result=%s\n",
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-temporal_started).count(),
            state.loaded?"ready":"failed");
        std::fflush(stdout);
      }
      if(state.loaded && !state.manifest->tiled && state.collision_out!=nullptr) {
        StartupWork::progress("player collision",&state.work);
        const auto collision_started=std::chrono::steady_clock::now();
        graphics::MapCollisionSoup soup{};
        *state.collision_out = {};
        if(graphics::open_world_renderer_map_collision(state.renderer,&soup)) {
          std::vector<float> positions;
          positions.reserve(soup.vertex_count*3u);
          for(std::size_t vertex=0;vertex<soup.vertex_count;++vertex) {
            const float* position=soup.positions+vertex*soup.stride_floats;
            positions.push_back(position[0]);
            positions.push_back(position[1]);
            positions.push_back(position[2]);
          }
          local_session::MeshCollisionSoup copy(std::move(positions),
              std::vector<std::uint32_t>(soup.indices,soup.indices+soup.index_count));
          local_session::Prediction warm_target;
          warm_target.set_collision(copy);
          warm_target.warm_collision();
          *state.collision_out = std::move(copy);
          graphics::open_world_renderer_release_map_geometry(state.renderer);
          std::printf("client_collision_ready triangles=%zu ms=%.1f\n",
              soup.index_count/3u,
              std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-collision_started).count());
          std::fflush(stdout);
        } else {
          std::fprintf(stderr,"client_collision unavailable reason=map_soup\n");
        }
      }
      StartupWork::progress("map ready",&state.work);
    } catch(const StartupWork::Cancelled&) {
    } catch(...) {state.failure=std::current_exception();}
    state.elapsed_ms=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-started).count();
    return 0;
  }
};
}

bool start_map(SDL_Window* window, graphics::WorldRenderer* renderer,
    const MapManifest& manifest, bool& running, local_session::MeshCollisionSoup& collision_out) {
  if(!running)return false;
  using Runtime=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_destroy)>;
  using Task=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_task_destroy)>;
  Runtime runtime(octaryn_native_schedule_runtime_create(SDL_GetNumLogicalCPUCores(),2),
      octaryn_native_schedule_runtime_destroy);
  if(!runtime)throw std::runtime_error("Cannot create map startup scheduler");
  MapStartup state;
  state.renderer=renderer;
  state.manifest=&manifest;
  state.collision_out=&collision_out;
  pump_boot_stage(window,"Loading map assets");
  octaryn_native_schedule_runtime_job job{};
  job.job_id="map_startup";
  job.execute=MapStartup::execute;
  job.context=&state;
  // Task is destroyed before state on every exit, so no worker can outlive it.
  Task task(octaryn_native_schedule_runtime_submit_worker(runtime.get(),&job,1),
      octaryn_native_schedule_runtime_task_destroy);
  if(!task)throw std::runtime_error("Cannot schedule map startup");
  const auto window_id=SDL_GetWindowID(window);
  auto previous=SDL_GetTicksNS();
  Uint64 maximum_gap{};
  unsigned pumps{};
  while(!octaryn_native_schedule_runtime_task_ready(task.get())) {
    const auto now=SDL_GetTicksNS();
    if(now-previous>maximum_gap)maximum_gap=now-previous;
    previous=now;
    ++pumps;
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
          event.window.windowID==window_id)) {
        running=false;
        state.work.cancel();
        SDL_SetWindowTitle(window,"Octaryn | Finishing map load before closing");
      }
    }
    // No drawing, resizing, status reads or renderer teardown while the worker
    // exclusively owns it. SDL window events remain on the main thread.
    SDL_Delay(8);
  }
  octaryn_native_schedule_runtime_report report{};
  const int result=octaryn_native_schedule_runtime_task_result(task.get(),&report);
  task.reset();
  const auto final_gap=SDL_GetTicksNS()-previous;
  if(final_gap>maximum_gap)maximum_gap=final_gap;
  std::printf("map_boot responsiveness=1 worker_jobs=%zu event_pumps=%u max_event_gap_ms=%.2f worker_elapsed_ms=%.0f result=%s\n",
      report.worker_jobs,pumps,double(maximum_gap)/1e6,state.elapsed_ms,
      !running?"cancelled":state.failure || result!=0 || !state.loaded?"failed":"ready");
  std::fflush(stdout);
  if(state.failure)std::rethrow_exception(state.failure);
  if(result!=0)throw std::runtime_error("Map startup job failed");
  return running && state.loaded;
}
}
