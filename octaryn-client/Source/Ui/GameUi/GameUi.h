#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>

struct SDL_Window;
union SDL_Event;
struct runtime_controls;
namespace Rml { class Context; class RenderInterface; }
namespace octaryn::client::rendering { struct UiDrawData; }
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
  bool validate_inventory_contract();
  void show_inventory(bool creative = false);
  void show_pause_menu();
  bool modal_open() const;
  std::uint16_t selected_block() const;
  bool select_hotbar(unsigned slot);
  bool cycle_hotbar(int delta);
  bool pick_block(std::uint16_t block);
  bool take_drop_request(GameUiDropRequest& request);
  bool finish_drop(bool accepted,std::uint64_t command_id);
  bool apply_pickup(std::uint64_t grant_id,std::uint16_t block_id,std::uint32_t count);
  bool validation_request_item_drop(bool stack=false);
  std::uint32_t inventory_count(std::uint16_t block_id) const;
  std::uint64_t inventory_drop_watermark() const;
  std::uint64_t inventory_grant_watermark() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
