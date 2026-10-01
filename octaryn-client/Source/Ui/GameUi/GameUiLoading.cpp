#include "GameUiState.h"
#include "ActionAudio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace octaryn::client::app {
void GameUi::show_loading(const std::string& title)
{
  auto& s = *state_;
  s.loading_visible = true;
  s.loading_title = title.empty() ? "Opening world" : title;
  s.loading_status = "Starting...";
  s.loading_detail.clear();
  s.loading_fraction = -1;
  s.loading_started = SDL_GetTicks();
  s.loading_cancelable = s.loading_cancel_requested = s.loading_cancelling = false;
  s.loading_pointer = {};
  s.controls.display_menu.active = 1;
  s.lighting.visible = s.inventory_open = s.controls_open = s.fsr_open = false;
  if (s.library.document) s.library.document->Hide();
  s.sync_menu();
  s.sync_capture();
}
void GameUi::update_loading(const std::string& status, const std::string& detail, float fraction)
{
  auto& s = *state_;
  if (!s.loading_visible) return;
  s.loading_status = s.loading_cancelling ? "Cancelling..." : status;
  s.loading_detail = detail;
  s.loading_fraction = s.loading_cancelling || !std::isfinite(fraction) || fraction < 0 ?
      -1 : std::clamp(fraction, 0.0f, 1.0f);
  s.sync_loading();
}
void GameUi::set_loading_cancelable(bool cancelable)
{
  auto& s = *state_;
  s.loading_cancelable = s.loading_visible && !s.loading_cancelling && cancelable;
  s.sync_loading();
}
bool GameUi::take_loading_cancel() { return std::exchange(state_->loading_cancel_requested, false); }
void GameUi::hide_loading()
{
  auto& s = *state_;
  s.loading_visible = s.loading_cancelable = s.loading_cancel_requested = s.loading_cancelling = false;
  s.loading_pointer = {};
  s.sync_menu();
  s.sync_capture();
}
bool GameUi::loading_visible() const { return state_->loading_visible; }
bool GameUi::validation_cancel_loading()
{
  auto& s = *state_;
  if (!(SDL_GetWindowFlags(s.window) & SDL_WINDOW_HIDDEN) || !s.loading_visible ||
      !s.loading_cancelable || !s.document) return false;
  auto* cancel = s.document->GetElementById("loading-cancel");
  if (!cancel || !cancel->IsVisible(true) || cancel->HasAttribute("disabled")) return false;
  cancel->DispatchEvent("click", {});
  return s.loading_cancel_requested;
}
bool GameUi::loading_event(const SDL_Event& event, int pixel_width, int pixel_height)
{
  auto& s = *state_;
  if (!s.loading_visible) return false;
  auto& pointer = s.loading_pointer;
  if (pointer.width != pixel_width || pointer.height != pixel_height ||
      event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
    pointer.valid = pointer.pressed = pointer.hovered = false;
  }
  if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) pointer.pressed = pointer.hovered = false;
  auto cancel = [&] {
    if (!s.loading_cancelable || s.loading_cancelling) return;
    s.loading_cancel_requested = s.loading_cancelling = true;
    s.loading_cancelable = pointer.pressed = pointer.hovered = false;
    s.loading_status = "Cancelling...";
    s.loading_fraction = -1;
    audio::play_action_audio(s.audio_feedback.audio(), audio::ActionSound::UiClick);
  };
  if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_ESCAPE &&
      event.key.windowID == pointer.window_id) cancel();
  const bool motion = event.type == SDL_EVENT_MOUSE_MOTION;
  const bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
  const bool up = event.type == SDL_EVENT_MOUSE_BUTTON_UP;
  if (motion || down || up) {
    const auto window_id = motion ? event.motion.windowID : event.button.windowID;
    if (window_id != pointer.window_id) return false;
    const float x = (motion ? event.motion.x : event.button.x) * pointer.density;
    const float y = (motion ? event.motion.y : event.button.y) * pointer.density;
    const bool inside = pointer.valid && s.loading_cancelable && x >= pointer.left && x < pointer.right &&
        y >= pointer.top && y < pointer.bottom;
    if (motion) {
      const auto now = SDL_GetTicks();
      if (inside && !pointer.hovered && (!pointer.hover_at || now - pointer.hover_at >= 65)) {
        audio::play_action_audio(s.audio_feedback.audio(), audio::ActionSound::UiHover);
        pointer.hover_at = now;
      }
      pointer.hovered = inside;
    } else if (event.button.button == SDL_BUTTON_LEFT) {
      if (down) pointer.pressed = inside;
      else {
        const bool activate = pointer.pressed && inside;
        pointer.pressed = false;
        if (activate) cancel();
      }
    }
  }
  return event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP ||
      event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_MOUSE_WHEEL || motion || down || up;
}
void GameUi::State::cache_loading_input()
{
  auto& pointer = loading_pointer;
  pointer.valid = false;
  pointer.window_id = SDL_GetWindowID(window);
  if (!loading_visible || !loading_cancelable || !document) return;
  auto* cancel = document->GetElementById("loading-cancel");
  if (!cancel || !cancel->IsVisible(true) || cancel->HasAttribute("disabled")) return;
  const auto bounds = cancel->GetAbsoluteOffset(Rml::BoxArea::Border);
  const auto size = cancel->GetBox().GetSize(Rml::BoxArea::Border);
  const auto viewport = context->GetDimensions();
  pointer.left = bounds.x; pointer.top = bounds.y;
  pointer.right = bounds.x + size.x; pointer.bottom = bounds.y + size.y;
  pointer.width = viewport.x; pointer.height = viewport.y;
  pointer.density = SDL_GetWindowPixelDensity(window);
  pointer.valid = size.x > 0 && size.y > 0 && std::isfinite(pointer.density) && pointer.density > 0 &&
      std::isfinite(pointer.left) && std::isfinite(pointer.right) &&
      std::isfinite(pointer.top) && std::isfinite(pointer.bottom);
}
void GameUi::State::sync_loading()
{
  if (!document) return;
  const bool shown = loading_visible && controls.display_menu.active != 0;
  visible("loading-veil", shown);
  visible("loading-screen", shown);
  if (auto* menu = document->GetElementById("menu")) menu->SetClass("loading", shown);
  if (!shown) return;
  text("loading-title", loading_title);
  text("loading-status", loading_status);
  text("loading-detail", loading_detail);
  visible("loading-detail", !loading_detail.empty());
  const auto elapsed = (SDL_GetTicks() - loading_started) / 1000;
  char elapsed_text[64]{};
  std::snprintf(elapsed_text, sizeof(elapsed_text), "Elapsed %llu:%02llu",
      static_cast<unsigned long long>(elapsed / 60), static_cast<unsigned long long>(elapsed % 60));
  text("loading-elapsed", elapsed_text);
  const bool known = loading_fraction >= 0;
  visible("loading-progress", known);
  if (known) text("loading-progress", std::to_string(static_cast<unsigned>(loading_fraction * 100)) + "%");
  if (auto* bar = document->GetElementById("loading-bar-fill")) {
    bar->SetClass("indeterminate", !known);
    bar->SetProperty(Rml::PropertyId::Width, Rml::Property(known ? loading_fraction * 100 : 35, Rml::Unit::PERCENT));
  }
  visible("loading-actions", loading_cancelable || loading_cancelling);
  if (auto* cancel = document->GetElementById("loading-cancel")) {
    if (loading_cancelable) cancel->RemoveAttribute("disabled");
    else cancel->SetAttribute("disabled", "disabled");
  }
  text("loading-cancel", loading_cancelling ? "Cancelling..." : "Cancel");
}
}
