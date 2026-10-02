#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "WorldLibraryTypes.h"

struct SDL_Window;
union SDL_Event;
struct runtime_controls;
namespace Rml { class Context; class RenderInterface; }
namespace octaryn::client::rendering { struct UiDrawData; }
namespace octaryn::client::audio { struct ActionAudio; }
namespace octaryn::client::app {
class LightingPanel;
struct GameUiDropRequest {std::uint16_t block_id{};std::uint32_t count{};};
class GameUi {
public:
  GameUi(SDL_Window* window, Rml::RenderInterface* renderer,
         const std::filesystem::path& assets, runtime_controls& controls, LightingPanel& lighting,
         const std::filesystem::path& palette = {});
  ~GameUi();
  GameUi(const GameUi&) = delete;
  GameUi& operator=(const GameUi&) = delete;
  std::uint32_t event(const SDL_Event& event, int width, int height);
  void update(const rendering::UiDrawData& profile, unsigned atlas_tile, int width, int height);
  Rml::Context* context() const;
  void set_render_resolution(unsigned,unsigned,unsigned,unsigned);
  void show_fsr_settings();
  bool validate_fsr_contract();
  bool validate_contract();
  bool validate_item_target_contract();
  bool validate_inventory_contract();
  void show_inventory(bool creative = false);
  void show_pause_menu();
  void show_main_menu();
  void show_world_library();
  bool world_library_visible() const;
  void set_world_library(const std::vector<WorldLibraryEntry>& entries,const std::string& status,bool busy,bool cancelable=false,bool failed=false);
  bool take_world_library_action(WorldLibraryAction& action);
  bool validate_world_library_contract();
  bool validate_loading_contract();
  void set_audio_feedback(audio::ActionAudio* audio);
  bool validate_ui_audio_contract();
  // Map worlds have no block gameplay; hides the voxel hotbar/hud elements.
  void hide_voxel_hud();
  void show_loading(const std::string& title);
  // Module-declared notification toast; auto-hides after a few seconds.
  bool show_notification(const std::string& text);
  bool present_module_screen(const std::string& declaration,const std::string& fields);
  bool hide_module_screen(const std::string& id);
  // Local presentation actions go only to host.ui and the owning client module.
  bool poll_module_screen_action(std::string& action);
  bool validation_screen_action(const std::string& action);
  // A negative fraction means progress is unknown; percentages require measured counts.
  void update_loading(const std::string& status, const std::string& detail, float fraction);
  void set_loading_cancelable(bool cancelable);
  // Input-only while a worker owns the renderer; uses the last safely drawn cancel bounds.
  bool loading_event(const SDL_Event& event, int pixel_width, int pixel_height);
  bool validation_cancel_loading();
  bool take_loading_cancel();
  void hide_loading();
  bool loading_visible() const;
  bool retarget_palette(const std::filesystem::path& path);
  bool modal_open() const;
  std::uint16_t selected_item() const;
  bool select_hotbar(unsigned slot);
  bool cycle_hotbar(int delta);
  bool pick_block(std::uint16_t block);
  bool take_drop_request(GameUiDropRequest& request);
  bool finish_drop(bool accepted,std::uint64_t command_id);
  bool apply_pickup(std::uint64_t grant_id,std::uint16_t block_id,std::uint32_t count);
  // Crosshair item highlight, driven by server look-target module events.
  void show_item_target(std::uint32_t item_id, std::uint32_t count);
  void clear_item_target();
  // G pressed in the world; the session publishes one interact.use intent.
  bool take_interact_request();
  void enable_module_actions();
  bool queue_module_action(const std::string& action);
  // Authoritative gameplay intents only; never drains declared screen actions.
  bool take_module_action(std::string& action);
  bool validation_request_item_drop(bool stack=false);
  std::uint32_t inventory_count(std::uint16_t block_id) const;
  std::uint64_t inventory_drop_watermark() const;
  std::uint64_t inventory_grant_watermark() const;
  bool consume_ui_capture_request();
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
