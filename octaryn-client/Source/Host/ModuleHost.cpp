#include "ModuleHost.h"
#include "../App/Startup/StartupWork.h"
#include <optional>
#include <type_traits>

#if defined(OCTARYN_CLIENT_REMOTE_MANAGED)

#include "ActionAudio.h"
#include "ActionRequest.h"
#include "GameUi.h"
#include "GraphicsHost.h"
#include "WorldRenderer.h"
#include "SceneTransitionHost.h"
#include "HostExports.h"
#include "LocalSession.h"
#include "octaryn_scene_physics.h"

#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_init.h>

#include <cstdio>
#include <cstring>

namespace octaryn::client::host {
namespace {

thread_local app::StartupWork* s_startup_work{};
template<class Function> auto startup_call(Function function) {
  using Result=std::invoke_result_t<Function>;std::optional<Result> result;
  struct Operation {Function& function;std::optional<Result>& result;} operation{function,result};
  try {
    app::StartupWork::main_thread([](void* data) {
      auto& operation=*static_cast<Operation*>(data);operation.result=operation.function();
    },&operation,s_startup_work);
    return *result;
  }catch(...) {
    std::fputs("module_boot_host_operation_failed owner=main_thread\n",stderr);
    if constexpr(std::is_pointer_v<Result>)return nullptr;else return Result(-1);
  }
}
ModuleHostHooks s_hooks;
int32_t scene_physics_submit(const uint8_t* request) {
  if(s_startup_work)return startup_call([&]{return scene_physics_submit(request);});
  if(!request || !SDL_IsMainThread() || !s_hooks.session)return -1;
  const auto* text=reinterpret_cast<const char*>(request);
  const auto size=std::char_traits<char>::length(text);
  if(size==0 || size>4096)return -1;
  return s_hooks.session->publish_scene_physics(std::string(text,size))?0:1;
}
int32_t scene_physics_snapshot(uint8_t* output,uint32_t capacity) {
  if(s_startup_work)return startup_call([&]{return scene_physics_snapshot(output,capacity);});
  if(!output || !capacity || !SDL_IsMainThread() || !s_hooks.session)return -1;
  std::string text;
  if(!s_hooks.session->scene_physics_snapshot(text))return 1;
  if(text.size()>=capacity)return -1;
  std::memcpy(output,text.c_str(),text.size()+1);return 0;
}
const octaryn_host_scene_physics_api s_scene_physics_api{1,sizeof(octaryn_host_scene_physics_api),
    scene_physics_submit,scene_physics_snapshot};
octaryn_host_input_snapshot s_last_input{};
uint64_t s_frame_index{};
bool s_ticked{};

double OCTARYN_ABI_CALL time_now_seconds() {
  return static_cast<double>(SDL_GetTicksNS()) / 1e9;
}

uint64_t OCTARYN_ABI_CALL time_tick_id() { return s_frame_index; }

double OCTARYN_ABI_CALL time_tick_rate() { return 60.0; }

void OCTARYN_ABI_CALL diagnostics_log_write(uint32_t level, const char* message) {
  static const char* const names[] = {"trace", "debug", "info", "warning", "error"};
  std::printf("module_api level=%s %s\n",
      level <= 4u ? names[level] : "unknown", message != nullptr ? message : "");
  std::fflush(stdout);
}

int OCTARYN_ABI_CALL input_poll(octaryn_host_input_snapshot* out_snapshot) {
  if(s_startup_work)return startup_call([&]{return input_poll(out_snapshot);});
  if (out_snapshot == nullptr || !s_ticked) return 1;
  *out_snapshot = s_last_input;
  return 0;
}

int OCTARYN_ABI_CALL audio_play_action_sound(
    uint64_t asset_id_hash, float volume, float x, float y, float z) {
  if(s_startup_work)return startup_call([&]{return audio_play_action_sound(asset_id_hash,volume,x,y,z);});
  audio::ActionSound sound{};
  if(!audio::action_sound_request(asset_id_hash,volume,x,y,z,sound))return -1;
  if (s_hooks.audio == nullptr) return -1;
  switch (play_action_audio(s_hooks.audio, sound,false,volume)) {
    case audio::PlayResult::Played: return 0;
    case audio::PlayResult::Busy: return 2;
    default: return -1;
  }
}

int OCTARYN_ABI_CALL audio_register_pcm(const uint8_t* data,uint32_t size,uint32_t rate,uint32_t channels,uint64_t* clip){
  if(s_startup_work)return startup_call([&]{return audio_register_pcm(data,size,rate,channels,clip);});
 if(!SDL_IsMainThread() || !data || !clip || !size || size>16777216)return -1;
 return audio::register_pcm16(s_hooks.audio,{data,size},rate,channels,*clip)?0:-1;
}
int OCTARYN_ABI_CALL audio_play_clip(uint64_t clip,float gain,uint32_t flags,float x,float y,float z,uint64_t* voice){
  if(s_startup_work)return startup_call([&]{return audio_play_clip(clip,gain,flags,x,y,z,voice);});
 if(!SDL_IsMainThread() || !voice)return -1;
 const auto result=audio::play_pcm_clip(s_hooks.audio,clip,gain,flags,x,y,z,*voice);return result==audio::PlayResult::Played?0:result==audio::PlayResult::Busy?2:-1;
}
int OCTARYN_ABI_CALL audio_stop_voice(uint64_t voice){
  if(s_startup_work)return startup_call([&]{return audio_stop_voice(voice);});return SDL_IsMainThread() && audio::stop_pcm_voice(s_hooks.audio,voice)?0:-1;}
int OCTARYN_ABI_CALL audio_release_clip(uint64_t clip){
  if(s_startup_work)return startup_call([&]{return audio_release_clip(clip);});return SDL_IsMainThread() && audio::release_pcm_clip(s_hooks.audio,clip)?0:-1;}

int OCTARYN_ABI_CALL audio_query_voice(uint64_t voice,uint32_t* out){
  if(s_startup_work)return startup_call([&]{return audio_query_voice(voice,out);});
 if(!SDL_IsMainThread() || !out)return -1;bool playing{};if(!audio::query_pcm_voice(s_hooks.audio,voice,playing))return -1;*out=playing?1u:0u;return 0;
}

int OCTARYN_ABI_CALL ui_show_notification(const char* text_utf8) {
  if(s_startup_work)return startup_call([&]{return ui_show_notification(text_utf8);});
  if (s_hooks.ui == nullptr || text_utf8 == nullptr) return -1;
  return s_hooks.ui->show_notification(text_utf8)?0:-1;
}

int OCTARYN_ABI_CALL ui_poll_action(char* buffer, uint32_t capacity) {
  if(s_startup_work)return startup_call([&]{return ui_poll_action(buffer,capacity);});
  if(!s_hooks.ui || !buffer || capacity<256)return -1;
  std::string action;
  if(!s_hooks.ui->poll_module_screen_action(action))return 1;
  if(action.size()>=capacity)return -1;
  std::memcpy(buffer,action.c_str(),action.size()+1);return 0;
}
int OCTARYN_ABI_CALL ui_present_screen(const char* declaration,const char* fields) {
  if(s_startup_work)return startup_call([&]{return ui_present_screen(declaration,fields);});
  if(!s_hooks.ui || !declaration || !fields)return -1;
  return s_hooks.ui->present_module_screen(declaration,fields)?0:-1;
}
int OCTARYN_ABI_CALL ui_hide_screen(const char* id) {
  if(s_startup_work)return startup_call([&]{return ui_hide_screen(id);});
  return s_hooks.ui && id && s_hooks.ui->hide_module_screen(id)?0:-1;
}
int OCTARYN_ABI_CALL residency_set(const uint32_t* wanted,uint32_t wanted_count,const uint32_t* retained,uint32_t retained_count) {
  if(s_startup_work)return startup_call([&]{return residency_set(wanted,wanted_count,retained,retained_count);});
 if(wanted_count>65536 || retained_count>65536 || (wanted_count && !wanted) || (retained_count && !retained) ||
     !SDL_IsMainThread() || !s_hooks.graphics)return -1;
 return rendering::open_world_renderer_set_desired_regions(s_hooks.graphics->renderer,{wanted,wanted_count},{retained,retained_count})?0:1;
}
int OCTARYN_ABI_CALL residency_actor(octaryn_host_region_anchor* out) {
  if(s_startup_work)return startup_call([&]{return residency_actor(out);});
 if(!out || !SDL_IsMainThread() || !s_hooks.graphics)return -1;
 return rendering::open_world_renderer_actor_position(s_hooks.graphics->renderer,*out)?0:1;
}
int OCTARYN_ABI_CALL residency_count(uint32_t* count,uint64_t* generation) {
  if(s_startup_work)return startup_call([&]{return residency_count(count,generation);});
 if(!count || !generation || !SDL_IsMainThread() || !s_hooks.graphics)return -1;
 return rendering::open_world_renderer_region_count(s_hooks.graphics->renderer,*count,*generation)?0:1;
}
int OCTARYN_ABI_CALL residency_query(uint32_t index,octaryn_host_region_status* out) {
  if(s_startup_work)return startup_call([&]{return residency_query(index,out);});
 if(!out || out->version!=1 || out->size!=OCTARYN_HOST_REGION_STATUS_SIZE || !SDL_IsMainThread() || !s_hooks.graphics)return -1;
 return rendering::open_world_renderer_region_status(s_hooks.graphics->renderer,index,*out)?0:1;
}
int OCTARYN_ABI_CALL graphics_read(octaryn_host_graphics_settings* settings) {
  if(s_startup_work)return startup_call([&]{return graphics_read(settings);});return graphics_get(s_hooks.graphics,settings);}
int OCTARYN_ABI_CALL application_exit() {
  if(s_startup_work)return startup_call([&]{return application_exit();});
  if(!s_hooks.running || !SDL_IsMainThread())return -1;
  *s_hooks.running=false;
  std::puts("application_exit accepted=1 cleanup=normal");
  return 0;
}
int OCTARYN_ABI_CALL graphics_write(const octaryn_host_graphics_settings* settings,uint32_t persist) {
  if(s_startup_work)return startup_call([&]{return graphics_write(settings,persist);});
  return graphics_apply(s_hooks.graphics,settings,persist);
}

int OCTARYN_ABI_CALL enqueue_command(octaryn_host_command* command) {
  if(s_startup_work)return startup_call([&]{return enqueue_command(command);});
  // Module commands from the client have no consumer yet; report failure so
  // modules see the drop instead of a silent hole.
  (void)command;
  return 0;
}

const octaryn_host_time_api s_time_api = {
    OCTARYN_HOST_TIME_API_VERSION, OCTARYN_HOST_TIME_API_SIZE,
    time_now_seconds, time_tick_id, time_tick_rate};
const octaryn_host_diagnostics_api s_diagnostics_api = {
    OCTARYN_HOST_DIAGNOSTICS_API_VERSION, OCTARYN_HOST_DIAGNOSTICS_API_SIZE,
    diagnostics_log_write};
const octaryn_host_input_api s_input_api = {
    OCTARYN_HOST_INPUT_API_VERSION, OCTARYN_HOST_INPUT_API_SIZE, input_poll};
const octaryn_host_audio_api s_audio_api = {
    OCTARYN_HOST_AUDIO_API_VERSION, OCTARYN_HOST_AUDIO_API_SIZE,
    audio_play_action_sound,audio_register_pcm,audio_play_clip,audio_stop_voice,audio_release_clip,audio_query_voice};
const octaryn_host_ui_api s_ui_api = {
    OCTARYN_HOST_UI_API_VERSION, OCTARYN_HOST_UI_API_SIZE,
    ui_show_notification, ui_poll_action,ui_present_screen,ui_hide_screen};
const octaryn_host_graphics_api s_graphics_api={OCTARYN_HOST_GRAPHICS_API_VERSION,OCTARYN_HOST_GRAPHICS_API_SIZE,
    graphics_read,graphics_write};
const octaryn_host_application_api s_application_api={OCTARYN_HOST_APPLICATION_API_VERSION,
    OCTARYN_HOST_APPLICATION_API_SIZE,application_exit};
int activation_transition_read(octaryn_host_transition_view* out) {return s_startup_work?startup_call([&]{return scene_transition_read(out);}):scene_transition_read(out);}
int activation_transition_begin(const char* asset,const char* descriptor,const octaryn_host_transition_pose* pose,uint64_t* revision) {return s_startup_work?startup_call([&]{return scene_transition_begin(asset,descriptor,pose,revision);}):scene_transition_begin(asset,descriptor,pose,revision);}
int activation_transition_status(uint64_t revision,uint32_t* state,char* error,uint32_t capacity) {return s_startup_work?startup_call([&]{return scene_transition_status(revision,state,error,capacity);}):scene_transition_status(revision,state,error,capacity);}
int activation_transition_cancel(uint64_t revision) {return s_startup_work?startup_call([&]{return scene_transition_cancel(revision);}):scene_transition_cancel(revision);}
const octaryn_host_transition_api s_transition_api={OCTARYN_HOST_TRANSITION_API_VERSION,OCTARYN_HOST_TRANSITION_API_SIZE,
    activation_transition_read,activation_transition_begin,activation_transition_status,activation_transition_cancel};

const octaryn_host_residency_api s_residency_api={1,OCTARYN_HOST_RESIDENCY_API_SIZE,residency_count,residency_query,residency_set,residency_actor};
const void* OCTARYN_ABI_CALL query_host_api(uint32_t api_id, uint32_t min_version) {
  switch (api_id) {
    case 16u:
      return min_version<=1?&s_scene_physics_api:nullptr;
    case OCTARYN_HOST_API_RESIDENCY:
      return s_hooks.graphics && min_version<=1?&s_residency_api:nullptr;
    case OCTARYN_HOST_API_TRANSITION:
      return min_version<=OCTARYN_HOST_TRANSITION_API_VERSION?&s_transition_api:nullptr;
    case OCTARYN_HOST_API_TIME:
      return min_version <= OCTARYN_HOST_TIME_API_VERSION ? &s_time_api : nullptr;
    case OCTARYN_HOST_API_DIAGNOSTICS:
      return min_version <= OCTARYN_HOST_DIAGNOSTICS_API_VERSION ? &s_diagnostics_api : nullptr;
    case OCTARYN_HOST_API_INPUT:
      return min_version <= OCTARYN_HOST_INPUT_API_VERSION ? &s_input_api : nullptr;
    case OCTARYN_HOST_API_AUDIO:
      return min_version <= OCTARYN_HOST_AUDIO_API_VERSION ? &s_audio_api : nullptr;
    case OCTARYN_HOST_API_UI:
      return min_version <= OCTARYN_HOST_UI_API_VERSION ? &s_ui_api : nullptr;
    case OCTARYN_HOST_API_GRAPHICS:
      return s_hooks.graphics && min_version<=OCTARYN_HOST_GRAPHICS_API_VERSION?&s_graphics_api:nullptr;
    case OCTARYN_HOST_API_APPLICATION:
      return s_hooks.running && min_version<=OCTARYN_HOST_APPLICATION_API_VERSION?&s_application_api:nullptr;
    default:
      return nullptr;
  }
}

bool s_started{};

} // namespace

bool module_host_active() {return s_started;}
bool module_host_rebind(const ModuleHostHooks& hooks) {
  if(!s_started || !SDL_IsMainThread())return false;
  s_hooks=hooks;return true;
}
int module_host_start(const ModuleHostHooks& hooks) {
  if (s_started) module_host_stop();
  s_hooks = hooks;
  s_ticked = false;

  octaryn_client_native_host_api api{};
  api.version = 1u;
  api.size = OCTARYN_CLIENT_NATIVE_HOST_API_SIZE;
  api.enqueue_command = enqueue_command;
  api.query_host_api = query_host_api;

  const int result = octaryn_client_initialize(&api);
  if (result != 0) {
    if(s_startup_work)startup_call([&]{audio::clear_pcm_audio(s_hooks.audio);return 0;});else audio::clear_pcm_audio(s_hooks.audio);
    std::fprintf(stderr, "module_host_start failed=%d\n", result);
    return result;
  }

  s_started = true;
  std::printf("module_host active=1 audio=%u ui=%u\n",
      hooks.audio != nullptr ? 1u : 0u, hooks.ui != nullptr ? 1u : 0u);
  return 0;
}

int module_host_start_worker(const ModuleHostHooks& hooks,app::StartupWork& work) {
  if(s_started)return -1;s_startup_work=&work;
  try {const auto result=module_host_start(hooks);s_startup_work=nullptr;return result;}
  catch(...) {s_startup_work=nullptr;throw;}
}

int module_host_tick(uint64_t frame_index, double delta_seconds,
                     const octaryn_host_input_snapshot& input) {
  if (!s_started) return 1;
  s_frame_index = frame_index;
  s_last_input = input;
  s_ticked = true;

  octaryn_host_frame_snapshot frame{};
  frame.version = 1u;
  frame.size = OCTARYN_HOST_FRAME_SNAPSHOT_SIZE;
  frame.input = input;
  frame.timing.version = 1u;
  frame.timing.size = OCTARYN_HOST_FRAME_TIMING_SNAPSHOT_SIZE;
  frame.timing.frame_index = frame_index;
  frame.timing.delta_seconds = delta_seconds;
  return octaryn_client_tick(&frame);
}

void module_host_stop() {
  if (!s_started) return;
  octaryn_client_shutdown();
  audio::clear_pcm_audio(s_hooks.audio);
  s_started = false;
  s_hooks = {};
}

} // namespace octaryn::client::host

#else

namespace octaryn::client::host {

int module_host_start(const ModuleHostHooks&) { return 1; }
int module_host_start_worker(const ModuleHostHooks&,app::StartupWork&) {return 1;}
bool module_host_active() {return false;}
bool module_host_rebind(const ModuleHostHooks&) {return false;}

int module_host_tick(uint64_t, double, const octaryn_host_input_snapshot&) { return 1; }

void module_host_stop() {}

} // namespace octaryn::client::host

#endif
