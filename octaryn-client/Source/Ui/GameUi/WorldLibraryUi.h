#pragma once
#include "WorldLibraryTypes.h"
#include <RmlUi/Core.h>
#include <deque>
#include <string>
#include <vector>

namespace octaryn::client::app {
struct WorldLibraryRow {
  std::string id,name,source,format,save_label,last_played;
  bool available{},selected{};
  bool preparation_required{};
};
struct WorldLibraryUi {
  Rml::ElementDocument* document{};
  Rml::DataModelHandle model;
  std::vector<WorldLibraryEntry> entries;
  std::vector<WorldLibraryRow> rows;
  std::vector<WorldLibrarySave> saves;
  std::deque<WorldLibraryAction> actions;
  std::string query,selected_id,name,source,format,save_label,last_played,status;
  std::string selected_save,open_label,new_save_label,prepare_label;
  WorldLibraryActionKind active_action{WorldLibraryActionKind::None};
  bool busy{},has_selection{},can_open{},missing{},empty{},no_matches{},multiple_saves{},has_saves{};
  bool preparation_required{},cancelable{},failed{},has_status{},single_action{};
};
}
