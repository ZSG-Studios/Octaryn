#include "MapStartup.h"
#include "StartupWork.h"
#include "WorldRenderer.h"
#include "LoadingScreen.h"
#include "LocalSession.h"
#include "Prediction.h"
#include "MapManifest.h"
#include "MapModel.h"
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
  MapManifest resolved_manifest;
  std::filesystem::path world,bundle;
  local_session::MeshCollisionSoup* collision_out{};
  std::exception_ptr failure;
  std::exception_ptr presentation_failure;
  std::function<bool(const std::string&,bool)> present;
  bool ui_safe{};
  bool loaded{};
  double elapsed_ms{};

  static void progress(const char* stage,bool cpu_only,void* user) {
    auto& state=*static_cast<MapStartup*>(user);
    struct Update {MapStartup& state;const char* stage;bool cpu_only;} update{state,stage,cpu_only};
    const auto frame=[](void* data) {
      auto& update=*static_cast<Update*>(data);
      auto& state=update.state;
      state.ui_safe=false;
      if(state.presentation_failure)std::rethrow_exception(state.presentation_failure);
      if(state.present && !state.work.cancelled() && !state.present(update.stage,true))state.work.cancel();
      state.ui_safe=update.cpu_only && !state.work.cancelled();
      std::printf("world_load_stage stage=%s ui_safe=%u\n",update.stage,unsigned(state.ui_safe));
      std::fflush(stdout);
    };
    StartupWork::main_thread(frame,&update,&state.work);
    StartupWork::progress(stage,&state.work);
  }

  static int execute(void* user) noexcept {
    auto& state=*static_cast<MapStartup*>(user);
    const auto started=std::chrono::steady_clock::now();
    try {
      if(!state.world.empty()) {
        progress("Checking selected save",true,&state);
        if(!load_world_manifest(state.world,state.bundle,state.resolved_manifest))
          throw std::runtime_error("This save's world files could not be opened.");
        state.manifest=&state.resolved_manifest;
      }
      progress("Preparing world items",false,&state);
      if(!graphics::open_world_renderer_prepare_items(state.renderer))
        throw std::runtime_error("World items could not be loaded.");
      progress("Loading map",false,&state);
      const auto path=(state.manifest->tiled?state.manifest->manifest:state.manifest->glb).generic_u8string();
      const auto catalog=state.manifest->scene_catalog.generic_u8string();
      state.loaded=!catalog.empty()
          ? graphics::open_world_renderer_load_scene(state.renderer,reinterpret_cast<const char*>(catalog.c_str()),reinterpret_cast<const char*>(path.c_str()))
          : state.manifest->tiled
          ? graphics::open_world_renderer_load_tiles(state.renderer,reinterpret_cast<const char*>(path.c_str()))
          : graphics::open_world_renderer_load_map(state.renderer,reinterpret_cast<const char*>(path.c_str()));
      const auto streaming_collision=state.loaded?graphics::open_world_renderer_tile_collision(state.renderer):nullptr;
      const bool streamed=state.manifest->tiled || bool(streaming_collision);
      if(state.loaded && streamed) {
        graphics::WorldCamera anchor{};
        anchor.x=state.manifest->spawn_x;anchor.y=state.manifest->spawn_y;anchor.z=state.manifest->spawn_z;
        graphics::open_world_renderer_set_tile_anchor(state.renderer,anchor);
        while(!graphics::open_world_renderer_tile_collision_ready(state.renderer,anchor.x,anchor.y,anchor.z)) {
          progress("Loading world region",false,&state);
          if(std::chrono::steady_clock::now()-started>std::chrono::seconds(60))
            throw std::runtime_error("Spawn world residency timed out");
          if(!graphics::open_world_renderer_prepare_tiles(state.renderer,anchor))
            throw std::runtime_error(std::string("Spawn world preparation failed: ")+graphics::open_world_renderer_status(state.renderer));
          SDL_Delay(1);
        }
        if(state.collision_out) *state.collision_out=local_session::MeshCollisionSoup(
            graphics::open_world_renderer_tile_collision(state.renderer),
            [renderer=state.renderer](float x,float y,float z,float radius) {
              return graphics::open_world_renderer_tile_collision_ready(renderer,x,y,z,radius);
            });
      }
      if(state.loaded) {
        progress("Preparing presentation",false,&state);
        const auto temporal_started=std::chrono::steady_clock::now();
        state.loaded=graphics::open_world_renderer_prepare_temporal(state.renderer);
        std::printf("map_boot_temporal ms=%.1f result=%s\n",
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-temporal_started).count(),
            state.loaded?"ready":"failed");
        std::fflush(stdout);
      }
      if(state.loaded && !streamed && state.collision_out!=nullptr) {
        progress("Preparing player collision",true,&state);
        const auto collision_started=std::chrono::steady_clock::now();
        graphics::MapCollisionSoup soup{};
        graphics::MapModel source_collision;
        bool collision_ready=false;
        if(!state.manifest->scene_catalog.empty()) {
          graphics::MapLoadLimits limits;limits.source_bytes=64ull*1024*1024;limits.encoded_bytes=64ull*1024*1024;
          limits.geometry_bytes=64ull*1024*1024;limits.triangles=250000;
          std::string error;
          if(!graphics::load_map_model(state.manifest->glb,source_collision,error,limits))
            throw std::runtime_error("Scene collision preparation failed: "+error);
          soup.positions=source_collision.vertices.empty()?nullptr:source_collision.vertices.front().position;
          soup.stride_floats=sizeof(graphics::MapVertex)/sizeof(float);soup.vertex_count=source_collision.vertices.size();
          soup.indices=source_collision.indices.data();soup.index_count=source_collision.indices.size();
          collision_ready=soup.positions && soup.index_count;
        } else collision_ready=graphics::open_world_renderer_map_collision(state.renderer,&soup);
        *state.collision_out = {};
        if(collision_ready) {
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
          progress("Finishing world load",false,&state);
          graphics::open_world_renderer_release_map_geometry(state.renderer);
          std::printf("client_collision_ready triangles=%zu ms=%.1f\n",
              soup.index_count/3u,
              std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-collision_started).count());
          std::fflush(stdout);
        } else {
          std::fprintf(stderr,"client_collision unavailable reason=map_soup\n");
        }
      }
      progress("Waiting for player",false,&state);
    } catch(const StartupWork::Cancelled&) {
    } catch(...) {state.failure=std::current_exception();}
    const auto relinquish=[](void* user) {static_cast<MapStartup*>(user)->ui_safe=false;};
    try {StartupWork::main_thread(relinquish,&state,&state.work);} catch(...) {state.failure=std::current_exception();}
    state.elapsed_ms=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-started).count();
    return 0;
  }
};
}

static bool run_map_startup(SDL_Window* window,MapStartup& state,bool& running) {
  if(!running)return false;
  using Runtime=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_destroy)>;
  using Task=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_task_destroy)>;
  Runtime runtime(octaryn_native_schedule_runtime_create(SDL_GetNumLogicalCPUCores(),2),
      octaryn_native_schedule_runtime_destroy);
  if(!runtime)throw std::runtime_error("Cannot create map startup scheduler");
  graphics::open_world_renderer_set_load_progress(state.renderer,MapStartup::progress,&state);
  struct FeedbackOwner {
    graphics::WorldRenderer* renderer;
    ~FeedbackOwner() {graphics::open_world_renderer_set_load_progress(renderer,nullptr,nullptr);}
  } feedback{state.renderer};
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
  auto last_frame=previous;
  Uint64 maximum_gap{};
  unsigned pumps{};
  while(!octaryn_native_schedule_runtime_task_ready(task.get())) {
    const auto now=SDL_GetTicksNS();
    if(now-previous>maximum_gap)maximum_gap=now-previous;
    previous=now;
    ++pumps;
    if(!state.present) {
      SDL_Event event;
      while(SDL_PollEvent(&event)) {
        if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
            event.window.windowID==window_id)) {
          running=false;state.work.cancel();
        }
      }
    }
    const auto stage=state.work.pump();
    if(state.present && !state.presentation_failure) {
      const bool draw=state.ui_safe && now-last_frame>=1000000000ull/30;
      try {
        if(!state.present(state.work.cancelled()?"Stopping...":stage,draw))state.work.cancel();
        if(draw)last_frame=now;
      }catch(...) {state.presentation_failure=std::current_exception();state.work.cancel();}
    }
    if(!running)state.work.cancel();
    SDL_Delay(8);
  }
  octaryn_native_schedule_runtime_report report{};
  const int result=octaryn_native_schedule_runtime_task_result(task.get(),&report);
  task.reset();
  const auto final_gap=SDL_GetTicksNS()-previous;
  if(final_gap>maximum_gap)maximum_gap=final_gap;
  std::printf("map_boot responsiveness=1 worker_jobs=%zu event_pumps=%u max_event_gap_ms=%.2f worker_elapsed_ms=%.0f result=%s\n",
      report.worker_jobs,pumps,double(maximum_gap)/1e6,state.elapsed_ms,
      !running || state.work.cancelled()?"cancelled":state.failure || result!=0 || !state.loaded?"failed":"ready");
  std::fflush(stdout);
  if(state.failure)std::rethrow_exception(state.failure);
  if(state.presentation_failure)std::rethrow_exception(state.presentation_failure);
  if(result!=0)throw std::runtime_error("Map startup job failed");
  return running && !state.work.cancelled() && state.loaded;
}
bool start_map(SDL_Window* window,graphics::WorldRenderer* renderer,const MapManifest& manifest,
    bool& running,local_session::MeshCollisionSoup& collision_out,
    std::function<bool(const std::string&,bool)> present) {
  MapStartup state;state.renderer=renderer;state.manifest=&manifest;
  state.collision_out=&collision_out;state.present=std::move(present);
  return run_map_startup(window,state,running);
}
MapStartupOutcome start_world_map(SDL_Window* window,graphics::WorldRenderer* renderer,const std::filesystem::path& world,
    const std::filesystem::path& bundle,MapManifest& manifest,bool& running,
    local_session::MeshCollisionSoup& collision_out,std::function<bool(const std::string&,bool)> present) {
  MapStartup state;state.renderer=renderer;state.world=world;state.bundle=bundle;
  state.collision_out=&collision_out;state.present=std::move(present);
  MapStartupOutcome outcome;
  try {outcome.ready=run_map_startup(window,state,running);}
  catch(const std::exception& error) {outcome.error=error.what();}
  outcome.cancelled=state.work.cancelled();
  if(outcome.ready)manifest=std::move(state.resolved_manifest);
  return outcome;
}
}
