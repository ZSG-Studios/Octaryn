#include "ModuleGameUiState.h"
#include "RuntimeControls.h"
#include "LoadingPresentation.h"
#include <algorithm>
#include <stdexcept>

namespace octaryn::client::app {
GameUi::GameUi(SDL_Window* window,Rml::RenderInterface* renderer,const std::filesystem::path&,
    runtime_controls& controls,LightingPanel&,const std::filesystem::path&) : state_(std::make_unique<State>()) {
  if(!window || !renderer)throw std::runtime_error("Missing host UI window or render interface");
  auto& s=*state_;s.window=window;s.controls=&controls;s.system.SetWindow(window);
  Rml::SetSystemInterface(&s.system);Rml::SetRenderInterface(renderer);
  if(!Rml::Initialise())throw std::runtime_error("Host RmlUi initialization failed");
  s.initialized=true;int width{},height{};SDL_GetWindowSizeInPixels(window,&width,&height);
  s.context=Rml::CreateContext("module",{std::max(1,width),std::max(1,height)});
  if(!s.context)throw std::runtime_error("Host UI context creation failed");
  controls.display_menu.active=0;
  std::printf("ui_host owner=module core_game_content=0\n");
}
GameUi::~GameUi()=default;
Rml::Context* GameUi::context() const {return state_->context;}
void GameUi::update(const rendering::UiDrawData&,unsigned,int width,int height) {
  auto& s=*state_;s.context->SetDimensions({std::max(1,width),std::max(1,height)});
  s.documents.fit(*s.context);
  s.sync_capture();s.context->Update();
  if(s.documents.sync_selection(*s.context))s.context->Update();
  if(s.documents.reflow())s.context->Update();
}
std::uint32_t GameUi::event(const SDL_Event& event,int width,int height) {
  auto& s=*state_;s.context->SetDimensions({std::max(1,width),std::max(1,height)});
  if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST)s.context->ProcessMouseLeave();
  const bool pressed=event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat;
  if(pressed && event.key.key==SDLK_F7){s.capture=true;return RUNTIME_CONTROLS_EVENT_CAPTURED;}
  if(s.loading)return loading_event(event,width,height)?RUNTIME_CONTROLS_EVENT_CAPTURED:0;
  const bool modal=s.documents.modal();
  SDL_Event forwarded=event;
  if(modal || !SDL_GetWindowRelativeMouseMode(s.window)) {
    if(event.type==SDL_EVENT_KEY_DOWN && (event.key.key==SDLK_TAB || event.key.key==SDLK_UP ||
        event.key.key==SDLK_DOWN || event.key.key==SDLK_LEFT || event.key.key==SDLK_RIGHT ||
        event.key.key==SDLK_RETURN || event.key.key==SDLK_KP_ENTER || event.key.key==SDLK_SPACE))s.documents.keyboard_input(*s.context);
    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP) {
      const float density=SDL_GetWindowPixelDensity(s.window);
      s.documents.pointer_position(*s.context,int(event.button.x*density),int(event.button.y*density),RmlSDL::GetKeyModifierState());
    }
    RmlSDL::InputEventHandler(s.context,s.window,forwarded);
    if(event.type==SDL_EVENT_MOUSE_MOTION)s.documents.pointer_move(*s.context);
  }
  s.sync_capture();
  if(modal)return RUNTIME_CONTROLS_EVENT_CAPTURED;
  // Game modules receive the menu edge through host.input; modal presentation
  // remembers the previous capture state and restores it when the menu closes.
  if(pressed && event.key.key==SDLK_ESCAPE)return RUNTIME_CONTROLS_EVENT_CAPTURED;
  if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || (pressed && (event.key.key==SDLK_F || event.key.key==SDLK_F5 ||
      event.key.key==SDLK_Z || event.key.key==SDLK_F3 || event.key.key==SDLK_F11))) {
    SDL_Event copy=event;return runtime_controls_handle_event(s.controls,s.window,&copy,width,height);
  }
  return 0;
}
bool GameUi::present_module_screen(const std::string& declaration,const std::string& fields) {
  const bool accepted=state_->documents.present(*state_->context,declaration,fields);
  if(accepted){state_->sync_capture();state_->context->Update();
    if(state_->documents.sync_selection(*state_->context))state_->context->Update();
    if(state_->documents.reflow())state_->context->Update();}return accepted;
}
bool GameUi::hide_module_screen(const std::string& id) {
  const bool accepted=state_->documents.hide(id);if(accepted)state_->sync_capture();return accepted;
}
bool GameUi::poll_module_screen_action(std::string& action) {return state_->documents.poll(action);}
bool GameUi::validation_screen_action(const std::string& action) {
  return (SDL_GetWindowFlags(state_->window)&SDL_WINDOW_HIDDEN) && state_->documents.dispatch_action(action);
}
bool GameUi::show_notification(const std::string&) {
  // Presentation is game-owned. No host-created toast is inserted into game documents.
  return false;
}
bool GameUi::modal_open() const {return state_->documents.modal();}
bool GameUi::consume_ui_capture_request() {const bool result=state_->capture;state_->capture=false;return result;}
void GameUi::show_loading(const std::string& title) {
  loading_presentation().begin();state_->loading=true;
  state_->loading_status.clear();state_->loading_detail.clear();
  std::fprintf(stderr,"host_loading title=%s\n",title.c_str());
}
void GameUi::update_loading(const std::string& status,const std::string& detail,float) {
  if(state_->loading_status==status && state_->loading_detail==detail)return;
  state_->loading_status=status;state_->loading_detail=detail;
  std::fprintf(stderr,"host_loading status=%s detail=%s\n",status.c_str(),detail.c_str());
}
void GameUi::set_loading_cancelable(bool value) {state_->cancelable=value;}
bool GameUi::loading_event(const SDL_Event& event,int,int) {
  if(!state_->loading)return false;
  if(state_->cancelable && event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key==SDLK_ESCAPE)state_->cancel=true;
  return true;
}
bool GameUi::take_loading_cancel() {const bool result=state_->cancel;state_->cancel=false;return result;}
void GameUi::hide_loading() {state_->loading=false;state_->cancelable=false;state_->cancel=false;}
bool GameUi::loading_visible() const {return state_->loading;}
void GameUi::enable_module_actions() {state_->actions.clear();}
bool GameUi::queue_module_action(const std::string& action) {
  if(action.empty() || action.size()>128 || state_->actions.size()>=64)return false;
  state_->actions.push_back(action);return true;
}
bool GameUi::take_module_action(std::string& action) {
  auto& pending=state_->actions;if(pending.empty())return false;
  action=std::move(pending.front());pending.pop_front();return true;
}
}
