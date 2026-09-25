#pragma once
#include "JumpTransitions.h"
#include <SDL3/SDL.h>

namespace octaryn::client::app {
class JumpInputEvents {
 JumpTransitions pending_;
 bool held_{}, suppressed_{};
 uint64_t overflows_{};
 void push(bool pressed) {
 if (pending_.count<pending_.pressed.size()) pending_.pressed[pending_.count++]=pressed;
 else { cancel(); ++overflows_; }
 }
public:
 void cancel() { pending_={}; pending_.reset=true; suppressed_=held_; }
 void event(const SDL_Event& event,bool allowed) {
 if (event.type==SDL_EVENT_WINDOW_FOCUS_LOST) {
 cancel(); held_=false; suppressed_=false; return;
 }
 if (!allowed) cancel();
 if ((event.type!=SDL_EVENT_KEY_DOWN && event.type!=SDL_EVENT_KEY_UP) ||
 event.key.scancode!=SDL_SCANCODE_SPACE) return;
 if (event.type==SDL_EVENT_KEY_DOWN) {
 if (event.key.repeat || held_) return;
 held_=true; suppressed_=!allowed;
 if (!suppressed_) push(true);
 } else {
 if (held_ && !suppressed_ && allowed) push(false);
 held_=false; suppressed_=false;
 }
 }
 JumpTransitions take() { const auto result=pending_; pending_={}; return result; }
 uint64_t overflows() const { return overflows_; }
};
}
