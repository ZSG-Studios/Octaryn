#pragma once
#include "GameUi.h"
#include "DeclaredDocumentUi.h"
#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <deque>

namespace octaryn::client::app {
struct ModuleUiSystem final : SystemInterface_SDL {
  bool LogMessage(Rml::Log::Type type,const Rml::String& message) override {
    std::fprintf(stderr,"rmlui severity=%u message=%s\n",unsigned(type),message.c_str());return true;
  }
};
struct GameUi::State {
  SDL_Window* window{};
  runtime_controls* controls{};
  ModuleUiSystem system;
  Rml::Context* context{};
  ui::DeclaredDocumentUi documents;
  std::deque<std::string> actions;
  bool initialized{},loading{},cancelable{},cancel{},capture{},mouse_relative{},previous_modal{};
  std::string loading_status,loading_detail;
  ~State() {
    documents.clear();
    if(initialized)Rml::Shutdown();
    Rml::SetSystemInterface(nullptr);Rml::SetRenderInterface(nullptr);
  }
  void sync_capture() {
    const bool modal=documents.modal();
    if((SDL_GetWindowFlags(window)&SDL_WINDOW_HIDDEN)==0) {
      if(modal && !previous_modal) {
        mouse_relative=SDL_GetWindowRelativeMouseMode(window);SDL_SetWindowRelativeMouseMode(window,false);
      } else if(!modal && previous_modal && mouse_relative)SDL_SetWindowRelativeMouseMode(window,true);
    }
    previous_modal=modal;
  }
};
}
