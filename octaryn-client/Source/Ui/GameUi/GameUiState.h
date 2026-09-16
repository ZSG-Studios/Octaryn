#pragma once
#include "GameUi.h"
#include "DebugMetrics.h"
#include "UiUpdateProfile.h"
#include "Inventory.h"
#include "RuntimeControls.h"
#include "LightingPanel.h"
#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <unordered_map>
#include <cstdio>

namespace octaryn::client::app {
struct UiSystem final : SystemInterface_SDL {
  unsigned warnings{}, errors{};
  bool LogMessage(Rml::Log::Type type,const Rml::String& message) override {
    if (type==Rml::Log::LT_WARNING) ++warnings;
    if (type==Rml::Log::LT_ERROR || type==Rml::Log::LT_ASSERT) ++errors;
    std::fprintf(stderr,"rmlui severity=%s message=%s\n",
      type==Rml::Log::LT_WARNING?"warning":(type==Rml::Log::LT_ERROR || type==Rml::Log::LT_ASSERT)?"error":"info",message.c_str());
    return true;
  }
};
struct GameUi::State final : Rml::EventListener {
  State(SDL_Window* window, runtime_controls& controls, LightingPanel& lighting);
  ~State();
  SDL_Window* window;
  runtime_controls& controls;
  LightingPanel& lighting;
  UiSystem system;
  Rml::Context* context{};
  Rml::ElementDocument* document{};
  bool initialized{}, mouse_was_relative{}, modal_was_open{};
  bool lighting_was_visible{};
  bool release_input_pending{};
  unsigned previous_screen{~0u};
  bool inventory_open{}, creative_open{}, controls_open{};
  unsigned previous_overlay{~0u};
  Inventory inventory;
  std::filesystem::path palette_path;
  std::string inventory_query;
  InventoryCategory inventory_category{InventoryCategory::All};
  std::uint64_t inventory_revision{~std::uint64_t{0}};
  bool creative_dirty{true};
  bool slots_initialized{},inventory_dragging{},suppress_inventory_click{};
  float cursor_x{},cursor_y{};
  GameUiDropRequest drop_request;
  bool drop_taken{};
  double inventory_toast_until{};
  std::uint64_t failed_pickup{},failed_pickup_revision{};
  double pickup_retry_at{};
  std::uint32_t pending{};
  bool ui_capture_requested{};
  unsigned last_tile{~0u};
  double metrics_at{-1};
  DebugMetrics metrics;
  UiUpdateProfile update_profile;
  std::unordered_map<std::string, std::string> text_cache;
  void ProcessEvent(Rml::Event& event) override;
  void text(const char* id, const std::string& value);
  void visible(const char* id, bool show);
  bool fsr_open{};
  bool fsr_syncing{};
  unsigned fsr_width{},fsr_height{},fsr_display_width{},fsr_display_height{};
  void sync_fsr();
  bool fsr_event(Rml::Event&,Rml::Element*);
  void sync_menu();
  void sync_lighting();
  void sync_capture();
  void release_input();
  bool modal_open() const;
  void open_inventory(bool creative);
  void close_inventory();
  void open_pause();
  void sync_inventory();
  void save_inventory();
  bool inventory_action(Rml::Element* target, const Rml::String& action);
  void inventory_hover(Rml::Element* target);
  bool inventory_pointer(Rml::Event& event,Rml::Element* target);
  void request_drop(bool stack);
  void position_cursor(float x,float y);
  void inventory_message(const char* message);
};
}
