#include "GameUiState.h"
#include "Menu.h"
#include <system_error>

namespace octaryn::client::app {
void GameUi::show_main_menu() { state_->open_main_menu(); }
void GameUi::hide_voxel_hud()
{
  auto& s = *state_;
  if (!s.document) return;
  for (const char* id : {"hotbar", "creative"})
    if (auto* element = s.document->GetElementById(id))
      element->SetProperty("display", "none");
}
bool GameUi::retarget_palette(const std::filesystem::path& path)
{
  auto& s = *state_;
  if (path.empty()) return false;
  if(!s.palette_path.empty() && !s.inventory.save_if_changed(s.palette_path))return false;
  Inventory next=s.inventory.fresh();
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if(error)return false;
  const bool exists=std::filesystem::exists(path,error);
  if(error || (exists && !next.load(path)))return false;
  // Abandon any in-flight drop/pickup against the previous world: its
  // command and grant identities are meaningless to the next authority.
  s.drop_request = {};
  s.drop_taken = false;
  s.failed_pickup = 0;
  s.failed_pickup_revision = 0;
  s.pickup_retry_at = 0;
  s.inventory=std::move(next);
  if(s.inventory.reserved_drop())s.inventory.resolve_drop(false);
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
void GameUi::State::return_to_menu()
{
  if (controls.session_active) open_pause();
  else open_main_menu();
}
}
