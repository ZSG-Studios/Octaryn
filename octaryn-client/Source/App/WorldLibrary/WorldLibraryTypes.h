#pragma once
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace octaryn::client::app {
struct MapManifest;
struct WorldLibrarySave {std::string id,name,last_played;};
struct WorldLibraryEntry {
  std::string id,name,source,format,save_label,last_played;
  bool available{};
  std::vector<WorldLibrarySave> saves;
  std::string active_save;
  bool preparation_required{};
};
enum class WorldLibraryActionKind {None,BrowseFiles,BrowseFolder,Open,NewSave,Locate,Prepare,Cancel};
struct WorldLibraryAction {WorldLibraryActionKind kind{};std::string world_id,save_id;};
class WorldLibrary {
public:
  WorldLibrary(std::filesystem::path root,std::filesystem::path bundle);
  ~WorldLibrary();
  WorldLibrary(const WorldLibrary&)=delete;
  WorldLibrary& operator=(const WorldLibrary&)=delete;
  bool refresh(std::string& error);
  const std::vector<WorldLibraryEntry>& entries() const;
  bool add_source(const std::filesystem::path&,std::string& error,const std::atomic_bool* cancel=nullptr);
  bool scan_folder(const std::filesystem::path&,std::string& error,const std::atomic_bool* cancel=nullptr);
  bool open_world(const std::string& id,std::filesystem::path& save_root,std::string& error,
      const std::atomic_bool* cancel=nullptr,std::function<void(const std::string&)> progress={});
  bool new_save(const std::string& id,std::filesystem::path& save_root,std::string& error,
      const std::atomic_bool* cancel=nullptr,std::function<void(const std::string&)> progress={});
  bool select_save(const std::string& world_id,const std::string& save_id,std::filesystem::path& save_root,std::string& error,
      const std::atomic_bool* cancel=nullptr,std::function<void(const std::string&)> progress={});
  bool locate(const std::string& id,const std::filesystem::path& source,std::string& error,const std::atomic_bool* cancel=nullptr);
  bool prepare(const std::string& id,std::string& error,const std::atomic_bool* cancel=nullptr,
      std::function<void(const std::string&)> progress={});
  static bool resolve(const std::filesystem::path& save_root,MapManifest&,std::string& error,const std::atomic_bool* cancel=nullptr);
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
