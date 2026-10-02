#pragma once
#if defined(OCTARYN_EXTERNAL_GAME_UI)
#include "MainMenu.h"
#include "OpenWorld.h"
#include "Controls.h"
#include "GameUi.h"
#include "GraphicsHost.h"
#include "UiData.h"
#include "MapTransitionLoading.h"

namespace octaryn::client::app {
// The reserved view identifies presentation before any authoritative scene.
// Its enabled flag admits a module-selected startup scene through the same
// verified transition mailbox used by in-game level changes.
inline constexpr const char* module_menu_scene="host.menu";

inline MenuEnd run_module_menu(MenuPhase& phase,MapManifest& manifest,
    local_session::MeshCollisionSoup& collision,std::uint64_t& module_frame,
    audio::ActionAudio* audio) {
  namespace graphics=octaryn::client::rendering;
  auto& controls=*phase.controls;auto& ui=*phase.ui;auto& session=*phase.session;
  static host::GraphicsHost graphics_host;
  graphics_host={phase.window,&controls.ui,phase.renderer};
  const host::ModuleHostHooks hooks{audio,&ui,&graphics_host,&controls.running};
  if(!host::module_host_active())host::scene_transition_reset();
  host::scene_transition_publish_view(module_menu_scene,{},true);
  const auto started=host::module_host_active()?(host::module_host_rebind(hooks)?0:-1):host::module_host_start(hooks);
  if(started!=0) {
    std::fprintf(stderr,"module_menu_start_failed result=%d\n",started);return MenuEnd::Failed;
  }
  controls.ui.session_active=0;ui.hide_loading();
  SDL_SetWindowTitle(phase.window,"Octaryn | Main Menu");
  std::puts("module_menu_ready scene_resident=0 authority_running=0");std::fflush(stdout);
  const auto* action=SDL_getenv("OCTARYN_CLIENT_MENU_ACTION");
  const auto* capture=SDL_getenv("OCTARYN_CLIENT_MENU_CAPTURE_PATH");
  if((action && *action) || (capture && *capture)) {
    if(!phase.options->startup_menu || !phase.options->benchmark_hidden || phase.options->frame_limit<=0)
      throw std::runtime_error("Menu qualification requires --startup-menu --benchmark-hidden --frames");
  }
  unsigned action_frame=120;
  if(const auto* frame=SDL_getenv("OCTARYN_CLIENT_MENU_ACTION_FRAME");frame && *frame) {
    const auto number=SDL_atoi(frame);
    if(number<1 || number>3600)throw std::runtime_error("Menu action frame requires1 through3600");
    action_frame=static_cast<unsigned>(number);
  }
  bool dispatched=false;
  std::uint64_t frames{};auto last=SDL_GetTicksNS();const auto began=last;
  const auto tick=[&] {
    octaryn_host_input_snapshot input{};input.version=1;input.size=OCTARYN_HOST_INPUT_SNAPSHOT_SIZE;
    input.flags=controls.menu_pressed?32u:0u;
    const auto now=SDL_GetTicksNS();
    const auto result=host::module_host_tick(module_frame++,double(now-last)/1e9,input);last=now;
    if(result!=0)throw std::runtime_error("Startup module tick failed");
  };
  while(controls.running) {
    const auto frame_started=SDL_GetTicksNS();
    read_world_controls(phase.window,controls,!phase.options->benchmark_hidden);
    if(!controls.running)break;
    tick();
    SDL_GetWindowSizeInPixels(phase.window,phase.width,phase.height);
    ui.update(graphics::make_ui_draw_data(controls.ui),0,*phase.width,*phase.height);
    graphics::open_world_renderer_set_present(phase.renderer,phase.options->benchmark_hidden?0:controls.ui.present_mode_index);
    if(!graphics::open_world_renderer_render_menu(phase.renderer))
      throw std::runtime_error("Startup menu presentation failed");
    ++frames;
    if(!dispatched && frames>=action_frame && ((action && *action) || (capture && *capture))) {
      if(capture && *capture) {
        const auto path=bundle_path(capture);
        if(!path.is_absolute() || path.extension()!=".bmp")
          throw std::runtime_error("Menu capture requires an absolute BMP path");
        std::filesystem::create_directories(path.parent_path());
        if(!graphics::open_world_renderer_capture_ui(phase.renderer,capture,true))
          throw std::runtime_error("Menu qualification capture failed");
      }
      if(action && *action && !ui.validation_screen_action(action))
        throw std::runtime_error("Menu qualification action is absent or disabled");
      dispatched=true;
      std::printf("module_menu_qualification frame=%llu action=%s captured=%u scene_resident=0 authority_running=0\n",
          static_cast<unsigned long long>(frames),action?action:"",unsigned(capture && *capture));std::fflush(stdout);
    }
    host::SceneTransitionRequest request;
    if(host::scene_transition_take(request)) {
      std::printf("module_menu_scene_selected revision=%llu asset=%s menu_frames=%llu\n",
          static_cast<unsigned long long>(request.revision),request.asset_id.c_str(),static_cast<unsigned long long>(frames));
      host::scene_transition_publish_view(module_menu_scene,{},false);
      ui.show_loading("Loading game");ui.set_loading_cancelable(false);
      const auto loading=[&](const std::string& stage,bool draw) {
        if(draw)tick();
        return present_loading(phase.window,phase.renderer,ui,controls,stage,"",false,draw);
      };
      try {
        // No world source or authority is opened before the game queues this
        // request. Loading UI is already presented before source resolution.
        loading("Opening selected game",true);
        *phase.world=default_world_path(phase.root,phase.bundle);
        auto next=resolve_scene_transition(request,*phase.world/"client",true);
        next.replace_scene=graphics::open_world_renderer_has_scene(phase.renderer);
        request.pose={next.spawn_x,next.spawn_y,next.spawn_z,next.yaw,next.pitch};
        host::scene_transition_mailbox.request.pose=request.pose;
        if(!start_map(phase.window,phase.renderer,next,controls.running,collision,loading))
          throw std::runtime_error("Startup scene could not be prepared");
        bool authority_started=false;
        loading_session_work([&] {
          authority_started=session.start(phase.bundle,*phase.world,*phase.radius,phase.root/"logs"/"server",&next);
        },loading,"Starting game authority");
        if(!authority_started)throw std::runtime_error("Startup authority failed: "+session.status());
        session.set_collision_mesh(collision);manifest=std::move(next);
        controls.yaw=request.pose.yaw;controls.pitch=request.pose.pitch;
        controls.ui.session_active=1;phase.active_endpoint->clear();
        std::printf("module_menu_scene_staged revision=%llu authority_running=1\n",static_cast<unsigned long long>(request.revision));
        // Completion remains pending until the first authoritative scene
        // frame is presented by MapWorldSession.
        return MenuEnd::WorldReady;
      } catch(const std::exception& error) {
        loading_session_work([&] {session.stop();},loading,"Stopping failed game");
        graphics::open_world_renderer_unload_map(phase.renderer);collision={};
        host::scene_transition_complete(request.revision,false,error.what());
        host::scene_transition_publish_view(module_menu_scene,{},true);
        controls.ui.session_active=0;ui.hide_loading();
        std::fprintf(stderr,"module_menu_scene_failed revision=%llu error=%s authority_running=0\n",
            static_cast<unsigned long long>(request.revision),error.what());
      }
    }
    if((phase.options->frame_limit>0 && frames>=static_cast<std::uint64_t>(phase.options->frame_limit)) ||
        (phase.options->benchmark_seconds>0 && double(SDL_GetTicksNS()-began)/1e9>=phase.options->benchmark_seconds))break;
    constexpr std::uint64_t interval=1000000000ull/60;
    const auto elapsed=SDL_GetTicksNS()-frame_started;
    if(elapsed<interval)SDL_DelayNS(interval-elapsed);
  }
  return MenuEnd::Quit;
}
}
#endif
