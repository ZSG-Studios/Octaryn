#pragma once
#include "WorldLibraryTypes.h"
#include <array>
#include <cstdint>
#include <optional>

namespace octaryn::client::app {
struct WorldSourceResource {std::string path,digest;uint64_t bytes{};int64_t modified{};};
struct WorldLibraryRecord {
  std::string id,name,source,manifest,format,active_save,last_played,legacy_save_directory;
  std::array<float,3> spawn{};
  float yaw{},pitch{};
  unsigned save_count{};
  std::vector<WorldSourceResource> resources;
  std::optional<bool> preparation_required;
  std::optional<bool> spawn_validated;
};
struct WorldLibraryCatalog {int version{1};std::vector<WorldLibraryRecord> worlds;};
struct WorldSaveDescriptor {int version{1};std::string world_id,save_id,name,last_played;};
struct WorldAuthoredTiles {std::vector<std::string> tile_files;std::vector<std::array<float,6>> tiles;};
struct WorldLibrary::State {
  std::filesystem::path root,bundle,directory;
  WorldLibraryCatalog catalog;
  std::vector<WorldLibraryEntry> visible;
  bool initialized{};
  bool read(std::string&);
  bool write(std::string&);
  void publish();
  WorldLibraryRecord* find(const std::string&);
  bool register_source(const std::filesystem::path&,bool authored,std::string&,const std::atomic_bool* cancel=nullptr);
  bool validate_source(WorldLibraryRecord&,std::string&,const std::atomic_bool*,
      const std::function<void(const std::string&)>&,bool preparing=false);
  bool create_save(WorldLibraryRecord&,std::filesystem::path&,std::string&,bool migrate,const std::atomic_bool*);
  bool activate_save(WorldLibraryRecord&,const std::string&,std::filesystem::path&,std::string&,const std::atomic_bool*);
};
std::string world_library_utf8(const std::filesystem::path&);
std::filesystem::path world_library_path(const std::string&);
std::string world_library_id();
bool world_library_valid_id(const std::string&);
bool world_library_load_catalog(const std::filesystem::path&,WorldLibraryCatalog&,std::string&);
bool world_library_describe(const std::filesystem::path&,WorldLibraryRecord&,std::string&,const std::atomic_bool* cancel=nullptr);
bool world_library_find_spawn(const std::filesystem::path&,std::array<float,3>&,std::string&,
    const std::atomic_bool* cancel=nullptr,const std::array<float,3>* authored=nullptr,const std::filesystem::path& manifest={});
bool world_library_read_save(const std::filesystem::path&,WorldSaveDescriptor&,std::string&);
bool world_library_resources(const std::filesystem::path&,WorldLibraryRecord&,std::string&,const std::atomic_bool* cancel=nullptr);
bool world_library_resources_current(const WorldLibraryRecord&,const std::atomic_bool* cancel=nullptr);
bool world_library_canceled(const std::atomic_bool*,std::string&);
bool world_library_preparation_required(const std::filesystem::path&);
bool world_library_prepared_scene(const std::filesystem::path& directory,const WorldLibraryRecord&,MapManifest&);
bool world_library_needs_preparation(const std::filesystem::path& directory,const WorldLibraryRecord&);
bool world_library_authored_tiles(const std::filesystem::path&,WorldAuthoredTiles&,std::string&);
}
