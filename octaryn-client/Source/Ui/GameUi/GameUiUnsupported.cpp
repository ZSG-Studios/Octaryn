#include "GameUi.h"

namespace octaryn::client::app {
// The host bridge retains these entrypoints for existing app call sites. Their
// first-party inventory/menu implementations are compiled only from basegame.
void GameUi::set_render_resolution(unsigned,unsigned,unsigned,unsigned) {}
void GameUi::show_fsr_settings() {}
bool GameUi::validate_fsr_contract() {return false;}
bool GameUi::validate_contract() {return false;}
bool GameUi::validate_item_target_contract() {return false;}
bool GameUi::validate_inventory_contract() {return false;}
void GameUi::show_inventory(bool) {}
void GameUi::show_pause_menu() {}
void GameUi::show_main_menu() {}
void GameUi::show_world_library() {}
bool GameUi::world_library_visible() const {return false;}
void GameUi::set_world_library(const std::vector<WorldLibraryEntry>&,const std::string&,bool,bool,bool) {}
bool GameUi::take_world_library_action(WorldLibraryAction&) {return false;}
bool GameUi::validate_world_library_contract() {return false;}
bool GameUi::validate_loading_contract() {return false;}
void GameUi::set_audio_feedback(audio::ActionAudio*) {}
bool GameUi::validate_ui_audio_contract() {return false;}
void GameUi::hide_voxel_hud() {}
bool GameUi::validation_cancel_loading() {return false;}
bool GameUi::retarget_palette(const std::filesystem::path&) {return false;}
std::uint16_t GameUi::selected_item() const {return 0;}
bool GameUi::select_hotbar(unsigned) {return false;}
bool GameUi::cycle_hotbar(int) {return false;}
bool GameUi::pick_block(std::uint16_t) {return false;}
bool GameUi::take_drop_request(GameUiDropRequest&) {return false;}
bool GameUi::finish_drop(bool,std::uint64_t) {return false;}
bool GameUi::apply_pickup(std::uint64_t,std::uint16_t,std::uint32_t) {return false;}
void GameUi::show_item_target(std::uint32_t,std::uint32_t) {}
void GameUi::clear_item_target() {}
bool GameUi::take_interact_request() {return false;}
bool GameUi::validation_request_item_drop(bool) {return false;}
std::uint32_t GameUi::inventory_count(std::uint16_t) const {return 0;}
std::uint64_t GameUi::inventory_drop_watermark() const {return 0;}
std::uint64_t GameUi::inventory_grant_watermark() const {return 0;}
}
