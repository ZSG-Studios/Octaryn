#include "GameUiState.h"
#include "Menu.h"
#include <algorithm>
#include <cmath>
#include <system_error>

namespace octaryn::client::app {
void GameUi::show_main_menu() { state_->open_main_menu(); }
void GameUi::show_loading(const std::string& title)
{
  auto& s = *state_;
  s.loading_visible = true;
  s.loading_title = title.empty() ? "Preparing world" : title;
  s.loading_status = "Starting...";
  s.loading_detail.clear();
  s.loading_fraction = 0;
  s.sync_menu();
  s.sync_capture();
}
void GameUi::update_loading(const std::string& status, const std::string& detail, float fraction)
{
  auto& s = *state_;
  if (!s.loading_visible) return;
  s.loading_status = status;
  s.loading_detail = detail;
  s.loading_fraction = std::clamp(fraction, 0.0f, 1.0f);
}
void GameUi::hide_loading()
{
  auto& s = *state_;
  s.loading_visible = false;
  s.sync_menu();
  s.sync_capture();
}
bool GameUi::loading_visible() const { return state_->loading_visible; }
bool GameUi::retarget_palette(const std::filesystem::path& path)
{
  auto& s = *state_;
  if (path.empty()) return false;
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  // Abandon any in-flight drop/pickup against the previous world: its
  // command and grant identities are meaningless to the next authority.
  s.drop_request = {};
  s.drop_taken = false;
  s.failed_pickup = 0;
  s.pickup_retry_at = 0;
  s.inventory.resolve_drop(false);
  if (std::filesystem::exists(path, error) && !s.inventory.load(path))
  {
    // Never save defaults over a damaged world palette.
    s.palette_path.clear();
    return false;
  }
  s.palette_path = path;
  s.inventory_revision = ~std::uint64_t{0};
  s.creative_dirty = true;
  s.sync_inventory();
  return true;
}
void GameUi::State::open_main_menu()
{
  if (!drop_request.count) inventory.cancel_move();
  inventory_open = false;
  creative_open = false;
  controls_open = false;
  fsr_open = false;
  loading_visible = false;
  lighting.visible = false;
  int width{}, height{};
  SDL_GetWindowSizeInPixels(window, &width, &height);
  runtime_controls_refresh_menu(&controls, window, width, height);
  display_menu_open_main(&controls.display_menu);
  sync_menu();
  sync_capture();
}
void GameUi::State::sync_loading()
{
  const bool shown = loading_visible && controls.display_menu.active != 0;
  visible("loading-veil", shown);
  visible("loading-screen", shown);
  if (!loading_visible) return;
  text("loading-title", loading_title);
  text("loading-status", loading_status);
  text("loading-detail", loading_detail);
  if (auto* bar = document->GetElementById("loading-bar-fill"))
  {
    const float percent = std::clamp(loading_fraction * 100.0f, 4.0f, 100.0f);
    bar->SetProperty(Rml::PropertyId::Width, Rml::Property(percent, Rml::Unit::PERCENT));
  }
}
void GameUi::State::return_to_menu()
{
  if (controls.session_active) open_pause();
  else open_main_menu();
}
}
