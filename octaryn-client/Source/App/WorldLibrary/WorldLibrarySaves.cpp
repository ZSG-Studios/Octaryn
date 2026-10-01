#include "WorldLibraryRecords.h"
#include "WorldLibraryIo.h"
#include "MapManifest.h"
#include "SessionFiles.h"
#include "MapTextureCache.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace octaryn::client::app {
struct RawMapDescriptor {int version{1};std::string map;std::array<float,3> spawn;float yaw{},pitch{};};
namespace {
std::string timestamp() {
  const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc,&now);
#else
  gmtime_r(&now,&utc);
#endif
  std::ostringstream text;text<<std::put_time(&utc,"%Y-%m-%dT%H:%M:%SZ");return text.str();
}
bool source_save_matches(const WorldLibraryRecord& record) {
  if(record.manifest.empty())return false;
  MapManifest manifest;std::error_code ec;
  return load_map_manifest_from(world_library_path(record.manifest),manifest) &&
      std::filesystem::equivalent(manifest.glb,world_library_path(record.source),ec) && !ec;
}
bool legacy_save_matches(const WorldLibraryRecord& record,const std::atomic_bool* cancel) {
  if(record.legacy_save_directory.empty() || !source_save_matches(record))return false;
  const auto current_root=world_library_path(record.manifest).parent_path();
  const auto legacy_root=world_library_path(record.legacy_save_directory);
  MapManifest legacy;
  if(!load_map_manifest_from(legacy_root/"map.json",legacy))return false;
  if(legacy.glb.lexically_normal().lexically_relative(legacy_root.lexically_normal())!=
      world_library_path(record.source).lexically_normal().lexically_relative(current_root.lexically_normal()))return false;
  // A matching filename alone cannot associate progress with a different map.
  for(const auto& resource:record.resources) {
    if(world_library_path(resource.path)==world_library_path(record.manifest))continue;
    const auto relative=world_library_path(resource.path).lexically_relative(current_root);
    if(relative.empty() || relative.is_absolute())return false;
    for(const auto& part:relative)if(part=="..")return false;
    std::error_code ec;const auto path=legacy_root/relative;
    if(std::filesystem::file_size(path,ec)!=resource.bytes || ec)return false;
    std::string error;
    if(world_library_canceled(cancel,error))return false;
    world_library_note_io(WorldLibraryIo::ResourceHash);
    if(rendering::map_texture_file_digest(path,error,64ull*1024*1024*1024,cancel)!=resource.digest)return false;
  }
  return true;
}
bool migrate_save(const std::filesystem::path& source,const std::filesystem::path& destination,std::string& error,const std::atomic_bool* cancel) {
  std::error_code ec;
  auto copy=[&](const std::filesystem::path& from,const std::filesystem::path& to) {
    if(world_library_canceled(cancel,error))return false;
    if(std::filesystem::exists(to,ec))return !ec;
    if(!std::filesystem::exists(from,ec))return !ec;
    if(ec || !std::filesystem::is_regular_file(from,ec))return false;
    std::filesystem::create_directories(to.parent_path(),ec);
    if(!ec)std::filesystem::copy_file(from,to,std::filesystem::copy_options::none,ec);
    return !ec;
  };
  if(!copy(source/"client"/"inventory.json",destination/"client"/"inventory.json") ||
      !copy(source/"world_time.json",destination/"world_time.json") ||
      !copy(source/"world-state.save",destination/"world-state.save")) {error="Existing world save could not be imported.";return false;}
  for(const auto& entry:std::filesystem::directory_iterator(source,ec)) {
    const auto name=entry.path().filename().string();
    if(name.starts_with("player_") && entry.path().extension()==".json" && !copy(entry.path(),destination/entry.path().filename())) {
      error="Existing player save could not be imported.";return false;
    }
  }
  if(ec) {error="Existing save directory could not be read.";return false;}
  return true;
}
}
bool WorldLibrary::State::create_save(WorldLibraryRecord& record,std::filesystem::path& save_root,std::string& error,bool migrate,const std::atomic_bool* cancel) {
  if(world_library_canceled(cancel,error))return false;
  if(record.save_count>=100000) {error="This world reached its save limit.";return false;}
  const auto save_id=world_library_id();const auto destination=directory/record.id/save_id;
  std::error_code ec;std::filesystem::create_directories(destination,ec);
  if(ec) {error="Save folder could not be created.";return false;}
  const auto rollback=[&] {
    // Both components are generated IDs, and the parent is the library's save directory.
    if(world_library_valid_id(record.id) && world_library_valid_id(save_id))std::filesystem::remove_all(destination,ec);
  };
  if(migrate && source_save_matches(record) &&
      !migrate_save(world_library_path(record.source).parent_path(),destination,error,cancel)) {rollback();return false;}
  if(migrate && legacy_save_matches(record,cancel) &&
      !migrate_save(world_library_path(record.legacy_save_directory),destination,error,cancel)) {rollback();return false;}
  if(world_library_canceled(cancel,error)) {rollback();return false;}
  std::string descriptor;
  if(glz::write_json(WorldSaveDescriptor{1,record.id,save_id,"Save "+std::to_string(record.save_count+1),timestamp()},descriptor) ||
      !local_session::write_text(destination/"world.json",descriptor)) {error="Save descriptor could not be written.";rollback();return false;}
  if(record.manifest.empty()) {
    RawMapDescriptor manifest{1,world_library_utf8(world_library_path(record.source).filename()),record.spawn,record.yaw,record.pitch};
    if(glz::write_json(manifest,descriptor) || !local_session::write_text(destination/"map.json",descriptor)) {
      error="World start position could not be saved.";rollback();return false;
    }
  }
  if(world_library_canceled(cancel,error)) {rollback();return false;}
  const auto prior=record;record.active_save=save_id;++record.save_count;record.last_played=timestamp();
  if(!write(error)) {record=prior;rollback();return false;}
  save_root=destination;return true;
}
bool WorldLibrary::open_world(const std::string& id,std::filesystem::path& save_root,std::string& error,
    const std::atomic_bool* cancel,std::function<void(const std::string&)> progress) {
  save_root.clear();error.clear();if(!state_->read(error))return false;
  auto* record=state_->find(id);if(!record) {error="World no longer exists in this library.";return false;}
  if(!state_->validate_source(*record,error,cancel,progress))return false;
  if(progress)progress("Opening your save...");
  if(record->active_save.empty())return state_->create_save(*record,save_root,error,true,cancel);
  return state_->activate_save(*record,record->active_save,save_root,error,cancel);
}
bool WorldLibrary::new_save(const std::string& id,std::filesystem::path& save_root,std::string& error,
    const std::atomic_bool* cancel,std::function<void(const std::string&)> progress) {
  save_root.clear();error.clear();if(!state_->read(error))return false;
  auto* record=state_->find(id);if(!record) {error="World no longer exists in this library.";return false;}
  if(!state_->validate_source(*record,error,cancel,progress))return false;
  if(progress)progress("Creating a separate save...");
  return state_->create_save(*record,save_root,error,false,cancel);
}
bool world_library_read_save(const std::filesystem::path& save_root,WorldSaveDescriptor& descriptor,std::string& error) {
  std::string text;
  constexpr glz::opts options{.error_on_unknown_keys=true,.error_on_missing_keys=true};
  if(!local_session::read_text(save_root/"world.json",text) || glz::read<options>(descriptor,text) || descriptor.version!=1 ||
      !world_library_valid_id(descriptor.world_id) || !world_library_valid_id(descriptor.save_id) ||
      save_root.filename()!=descriptor.save_id || save_root.parent_path().filename()!=descriptor.world_id ||
      descriptor.name.empty() || descriptor.name.size()>256) {
    error="World save descriptor is invalid.";return false;
  }
  return true;
}
bool WorldLibrary::select_save(const std::string& world_id,const std::string& save_id,std::filesystem::path& save_root,std::string& error,
    const std::atomic_bool* cancel,std::function<void(const std::string&)> progress) {
  save_root.clear();error.clear();if(!state_->read(error))return false;
  auto* record=state_->find(world_id);
  if(!record || !world_library_valid_id(save_id)) {error="World save no longer exists.";return false;}
  if(!state_->validate_source(*record,error,cancel,progress))return false;
  if(progress)progress("Opening your save...");
  return state_->activate_save(*record,save_id,save_root,error,cancel);
}
bool WorldLibrary::State::activate_save(WorldLibraryRecord& world,const std::string& save_id,
    std::filesystem::path& save_root,std::string& error,const std::atomic_bool* cancel) {
  if(world_library_canceled(cancel,error))return false;
  auto* record=&world;
  const auto destination=directory/world.id/save_id;
  WorldSaveDescriptor descriptor;MapManifest manifest;
  if(!world_library_read_save(destination,descriptor,error) || !WorldLibrary::resolve(destination,manifest,error,cancel))return false;
  if(world_library_canceled(cancel,error))return false;
  const auto previous=*record;const auto previous_descriptor=descriptor;
  record->active_save=save_id;record->last_played=timestamp();descriptor.last_played=record->last_played;
  std::string text;
  if(glz::write_json(descriptor,text) || !local_session::write_text(destination/"world.json",text)) {
    *record=previous;error="Save activity could not be recorded.";return false;
  }
  if(!write(error)) {
    *record=previous;if(!glz::write_json(previous_descriptor,text))local_session::write_text(destination/"world.json",text);
    return false;
  }
  save_root=destination;return true;
}
bool WorldLibrary::resolve(const std::filesystem::path& save_root,MapManifest& output,std::string& error,const std::atomic_bool* cancel) {
  error.clear();const auto marker=save_root/"world.json";std::error_code ec;
  if(world_library_canceled(cancel,error))return false;
  if(!std::filesystem::is_regular_file(marker,ec))return false;
  std::string text;WorldSaveDescriptor descriptor;
  if(!world_library_read_save(save_root,descriptor,error))return false;
  WorldLibraryCatalog catalog;
  if(!world_library_load_catalog(save_root.parent_path().parent_path()/"catalog.json",catalog,error))return false;
  const auto world=std::find_if(catalog.worlds.begin(),catalog.worlds.end(),[&](const auto& record){return record.id==descriptor.world_id;});
  if(world==catalog.worlds.end()) {error="This save belongs to an unknown world.";return false;}
  const auto source=world_library_path(world->source);
  if(world_library_needs_preparation(save_root.parent_path().parent_path(),*world)) {error="This world needs preparation before it can open.";return false;}
  if(!std::filesystem::is_regular_file(source,ec)) {error="World file is missing. Locate it to continue.";return false;}
  if(!world_library_resources_current(*world,cancel)) {
    if(!world_library_canceled(cancel,error))error="World files have changed or a resource is missing. Locate the world to update its source.";
    return false;
  }
  if(world_library_canceled(cancel,error))return false;
  MapManifest resolved;
  if(world_library_prepared_scene(save_root.parent_path().parent_path(),*world,resolved)) {
    output=std::move(resolved);return true;
  }
  if(!world->spawn_validated.value_or(true)) {error="This world needs its starting position checked. Select the world to continue.";return false;}
  if(!world->manifest.empty()) {
    if(!load_map_manifest_from(world_library_path(world->manifest),resolved)) {error="Authored world manifest is unavailable or invalid.";return false;}
    if(!std::filesystem::equivalent(source,resolved.glb,ec) || ec) {error="World manifest points at another source.";return false;}
  } else {
    resolved.glb=source;resolved.manifest=save_root/"map.json";
    resolved.spawn_x=world->spawn[0];resolved.spawn_y=world->spawn[1];resolved.spawn_z=world->spawn[2];
    resolved.yaw=world->yaw;resolved.pitch=world->pitch;
    RawMapDescriptor raw{1,world_library_utf8(source.filename()),world->spawn,world->yaw,world->pitch};
    if(glz::write_json(raw,text) || !local_session::write_text(resolved.manifest,text)) {
      error="World start manifest could not be prepared.";return false;
    }
  }
  output=std::move(resolved);return true;
}
}
