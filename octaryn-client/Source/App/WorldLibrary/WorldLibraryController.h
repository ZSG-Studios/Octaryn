#pragma once
#include "WorldLibrary.h"
#include <memory>
#include <vector>

struct SDL_Window;
namespace octaryn::client::app {
class GameUi;
class WorldLibraryController {
public:
  WorldLibraryController(const std::filesystem::path& root,const std::filesystem::path& bundle);
  ~WorldLibraryController();
  void update(GameUi& ui);
  void add_files(const std::vector<std::filesystem::path>& paths);
  void find_folder(const std::filesystem::path& folder);
  bool action(const WorldLibraryAction& action,SDL_Window* window,
      std::filesystem::path& requested);
  bool open_index(unsigned index,std::filesystem::path& requested);
  bool busy() const;
  bool loading() const;
  bool failed() const;
  const std::string& status() const;
  const std::string& selected_name() const;
  const std::string& selected_detail() const;
  bool take_opened(std::filesystem::path&,std::string& world_name,std::string& save_name);
  void message(const std::string& text,bool failed=true);
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
