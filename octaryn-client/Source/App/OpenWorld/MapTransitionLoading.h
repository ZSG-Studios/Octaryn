#pragma once
#include "MapSceneTransition.h"
#include "MapStartup.h"
#include "LoadingScreen.h"
#include "FrameTimingLog.h"
#include "Controls.h"
#include "GameUi.h"
#include "LocalSession.h"
#include "ModuleHost.h"
#include "WorldRenderer.h"
#include "octaryn_native_schedule_runtime.h"
#include <exception>
#include <functional>
#include <memory>

namespace octaryn::client::app {
// Session ownership stays on this worker until its joined task completes.
// Rendering and game-module ticks stay on the caller's main thread.
inline void loading_session_work(const std::function<void()>& operation,
    const std::function<bool(const std::string&,bool)>& present,const std::string& stage) {
  struct Work {
    const std::function<void()>& operation;
    std::exception_ptr failure;
    static int execute(void* argument) noexcept {
      auto& work=*static_cast<Work*>(argument);
      try {work.operation();} catch(...) {work.failure=std::current_exception();}
      return 0;
    }
  } work{operation,{}};
  using Runtime=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_destroy)>;
  using Task=std::unique_ptr<void,decltype(&octaryn_native_schedule_runtime_task_destroy)>;
  Runtime runtime(octaryn_native_schedule_runtime_create(1,1),octaryn_native_schedule_runtime_destroy);
  if(!runtime)throw std::runtime_error("Cannot create transition authority scheduler");
  octaryn_native_schedule_runtime_job job{};
  job.job_id="transition_authority";job.execute=Work::execute;job.context=&work;
  Task task(octaryn_native_schedule_runtime_submit_worker(runtime.get(),&job,1),
      octaryn_native_schedule_runtime_task_destroy);
  if(!task)throw std::runtime_error("Cannot schedule transition authority operation");
  auto last_frame=SDL_GetTicksNS();
  std::exception_ptr presentation_failure;
  while(!octaryn_native_schedule_runtime_task_ready(task.get())) {
    const auto now=SDL_GetTicksNS();
    const bool draw=now-last_frame>=1000000000ull/60;
    if(!presentation_failure) {
      try {present(stage,draw);if(draw)last_frame=now;}
      catch(...) {presentation_failure=std::current_exception();}
    }
    SDL_Delay(1);
  }
  octaryn_native_schedule_runtime_report report{};
  const auto result=octaryn_native_schedule_runtime_task_result(task.get(),&report);
  task.reset();
  if(work.failure)std::rethrow_exception(work.failure);
  if(presentation_failure)std::rethrow_exception(presentation_failure);
  if(result!=0)throw std::runtime_error("Transition authority operation failed");
}

inline bool replace_map_scene(SDL_Window* window,rendering::WorldRenderer* renderer,GameUi& ui,
    WorldControls& controls,LocalSession& session,MapManifest& manifest,
    local_session::MeshCollisionSoup& collision,const host::SceneTransitionRequest& request,
    const std::filesystem::path& world,const std::filesystem::path& bundle,
    const std::filesystem::path& logs,unsigned radius,bool remote,
    std::uint64_t& module_frame,FrameTimingLog& timing,std::string& restoration_error) {
  restoration_error.clear();
  auto previous=manifest;
  const auto source_pose=host::scene_transition_mailbox.view.pose;
  previous.spawn_x=float(source_pose.x);previous.spawn_y=float(source_pose.y);previous.spawn_z=float(source_pose.z);
  previous.yaw=source_pose.yaw;previous.pitch=source_pose.pitch;
  const auto began=request.queued_at_ns?request.queued_at_ns:SDL_GetTicksNS();
  auto last_tick=SDL_GetTicksNS(),last_present=last_tick;
  host::scene_transition_publish_view(previous.scene_asset,source_pose,false);
  ui.show_loading("Changing level");ui.set_loading_cancelable(false);
  const auto loading=[&](const std::string& stage,bool draw) {
    // draw=true is an exclusive renderer boundary. Do not tick graphics APIs
    // while the map worker owns the renderer during an upload or publication.
    if(draw) {
      octaryn_host_input_snapshot input{};input.version=1;input.size=OCTARYN_HOST_INPUT_SNAPSHOT_SIZE;
      const auto tick=SDL_GetTicksNS();
      host::module_host_tick(module_frame++,double(tick-last_tick)/1e9,input);last_tick=tick;
    }
    if(SDL_GetTicksNS()-began<150000000ull)return controls.running;
    const auto accepted=present_loading(window,renderer,ui,controls,stage,"",false,draw);
    if(draw) {
      const auto completed=SDL_GetTicksNS();frame_profile_sample sample{};
      sample.total_ms=float(double(completed-last_present)/1e6);last_present=completed;
      rendering::WorldCamera camera{};camera.x=request.pose.x;camera.y=request.pose.y;camera.z=request.pose.z;
      timing.frame(sample,rendering::open_world_renderer_stats(renderer),camera,"loading");
    }
    return accepted;
  };
  bool switched=false;
  std::string failure;
  try {
    loading("Leaving level",true);
    loading_session_work([&] {session.stop();},loading,"Leaving level");
    if(remote)throw std::runtime_error("Remote authority does not support local scene replacement");
    auto next=resolve_scene_transition(request,world/"client");
    if(!start_map(window,renderer,next,controls.running,collision,loading))
      throw std::runtime_error("Declared scene replacement failed");
    loading("Starting level authority",true);
    bool started=false;
    loading_session_work([&] {started=session.start(bundle,world,radius,logs,&next);},loading,"Starting level authority");
    if(!started)throw std::runtime_error("Scene authority startup failed: "+session.status());
    session.set_collision_mesh(collision);manifest=std::move(next);
    controls.yaw=request.pose.yaw;controls.pitch=request.pose.pitch;switched=true;
  } catch(const std::exception& error) {failure=error.what();}
  std::printf("scene_transition revision=%llu asset=%s staged=%u elapsed_ms=%.3f error=%s\n",
      static_cast<unsigned long long>(request.revision),request.asset_id.c_str(),unsigned(switched),
      double(SDL_GetTicksNS()-began)/1e6,failure.c_str());
  std::fflush(stdout);
  if(!switched) {
    try {
      previous.replace_scene=true;
      write_scene_spawn_manifest(previous,world/"client",std::to_string(request.revision)+"-restore");
      if(!controls.running || !start_map(window,renderer,previous,controls.running,collision,loading))
        throw std::runtime_error("Previous scene could not be restored");
      bool started=false;
      loading_session_work([&] {started=session.start(bundle,world,radius,logs,&previous);},loading,"Restoring level authority");
      if(!started)throw std::runtime_error("Previous scene authority could not be restored");
      session.set_collision_mesh(collision);manifest=previous;
      restoration_error=failure;
    } catch(const std::exception& error) {
      std::fprintf(stderr,"scene_transition_restore_failed error=%s\n",error.what());
      host::scene_transition_complete(request.revision,false,failure+"; restoration failed: "+error.what());
      return false;
    }
  }
  return true;
}
}
