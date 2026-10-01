#include "WorldLibraryRecords.h"
#include "MapManifest.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

namespace octaryn::client::app {
bool world_library_describe(const std::filesystem::path& input,WorldLibraryRecord& record,std::string& error,const std::atomic_bool* cancel) {
  if(world_library_canceled(cancel,error))return false;
  std::error_code ec;
  auto source=std::filesystem::weakly_canonical(std::filesystem::absolute(input,ec),ec);
  if(ec || !std::filesystem::is_regular_file(source,ec)) {error="Select an existing GLB or glTF file.";return false;}
  auto extension=source.extension().string();
  std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
  if(extension!=".glb" && extension!=".gltf") {error="Choose a .glb or .gltf world.";return false;}
  record.source=world_library_utf8(source);record.format=extension==".glb"?"GLB":"glTF";
  record.name=world_library_utf8(source.stem());
  MapManifest manifest;
  const auto sibling=source.parent_path()/"map.json";
  if(std::filesystem::is_regular_file(sibling,ec) && load_map_manifest_from(sibling,manifest) &&
      std::filesystem::equivalent(manifest.glb,source,ec) && !ec) {
    record.manifest=world_library_utf8(sibling);record.spawn={manifest.spawn_x,manifest.spawn_y,manifest.spawn_z};
    record.yaw=manifest.yaw;record.pitch=manifest.pitch;
    if(source.stem()=="main" || source.stem()=="map")record.name=source.parent_path().filename()=="Maps"?"Starter world":world_library_utf8(source.parent_path().filename());
    const auto label_path=source.parent_path()/"world_name.txt";
    const auto label_bytes=std::filesystem::file_size(label_path,ec);
    if(!ec && label_bytes<=1024) {
      std::ifstream name(label_path);std::string label;
      if(name && std::getline(name,label) && !label.empty() && label.size()<=256)record.name=label;
    }
  } else record.manifest.clear();
  if(record.name.empty() || record.name.size()>256) {error="World filename is too long.";return false;}
  return !world_library_canceled(cancel,error);
}
bool world_library_canceled(const std::atomic_bool* cancel,std::string& error) {
  if(!cancel || !cancel->load())return false;
  error="World loading canceled.";return true;
}
bool WorldLibrary::State::validate_source(WorldLibraryRecord& world,std::string& error,const std::atomic_bool* cancel,
    const std::function<void(const std::string&)>& progress,bool preparing) {
  if(world_library_canceled(cancel,error))return false;
  if(progress)progress("Checking the selected world...");
  if(world_library_canceled(cancel,error))return false;
  WorldLibraryRecord checked=world;
  if(!world_library_describe(world_library_path(world.source),checked,error,cancel))return false;
  checked.name=world.name;
  if(progress)progress("Verifying world resources...");
  if(!world_library_resources(world_library_path(checked.source),checked,error,cancel))return false;
  for(const auto& prior:world.resources) {
    const auto found=std::find_if(checked.resources.begin(),checked.resources.end(),[&](const auto& value){return value.path==prior.path;});
    if(found==checked.resources.end() || found->digest!=prior.digest) {
      error="World files have changed or a resource is missing. Locate the world to update its source.";return false;
    }
  }
  if(world_library_canceled(cancel,error))return false;
  const bool large=world_library_preparation_required(world_library_path(checked.source));
  MapManifest prepared;
  const bool ready=world_library_prepared_scene(directory,checked,prepared);
  checked.preparation_required=large && !ready;
  if(!preparing && !large && !ready && !world.spawn_validated.value_or(!world.resources.empty())) {
    if(progress)progress("Checking the starting position...");
    std::array<float,3> spawn{};
    if(!world_library_find_spawn(world_library_path(checked.source),spawn,error,cancel,
        checked.manifest.empty()?nullptr:&checked.spawn,world_library_path(checked.manifest)))return false;
    if(checked.manifest.empty())checked.spawn=spawn;
    checked.spawn_validated=true;
  }
  if(ready)checked.spawn_validated=true;
  if(world_library_canceled(cancel,error))return false;
  if(!world_library_resources_current(checked,cancel)) {
    if(!world_library_canceled(cancel,error))error="World files changed while loading. Try again after the files finish updating.";
    return false;
  }
  const auto previous=world;world=std::move(checked);
  if(!write(error)) {world=previous;return false;}
  if(!preparing && world.preparation_required.value_or(false)) {
    error="This world needs preparation before it can open. Select Prepare world.";return false;
  }
  return true;
}

bool WorldLibrary::State::register_source(const std::filesystem::path& source,bool authored,std::string& error,const std::atomic_bool* cancel) {
  WorldLibraryRecord next;
  if(world_library_canceled(cancel,error))return false;
  std::error_code ec;const auto canonical=std::filesystem::weakly_canonical(source,ec);
  if(ec || !std::filesystem::is_regular_file(canonical,ec)) {error="Select an existing GLB or glTF file.";return false;}
  if(!ec)for(const auto& world:catalog.worlds) {
    std::error_code same_error;
    if(canonical==world_library_path(world.source) ||
        (std::filesystem::equivalent(canonical,world_library_path(world.source),same_error) && !same_error))return true;
  }
  if(catalog.worlds.size()>=4096) {error="World library reached its 4096-world limit.";return false;}
  if(!world_library_describe(source,next,error,cancel))return false;
  if(authored && !next.manifest.empty()) {
    const auto assets=std::filesystem::weakly_canonical(root/"octaryn-client"/"Assets"/"Maps",ec);
    if(!ec) {
      const auto relative=canonical.lexically_relative(assets);
      bool inside=!relative.empty();for(const auto& part:relative)inside=inside && part!="..";
      const auto legacy=bundle/"Client"/"Assets"/"Maps"/relative.parent_path();
      MapManifest legacy_manifest;
      if(inside && std::filesystem::is_regular_file(legacy/"map.json",ec) &&
          load_map_manifest_from(legacy/"map.json",legacy_manifest) &&
          legacy_manifest.glb.lexically_normal().lexically_relative(legacy.lexically_normal())==canonical.filename())
        next.legacy_save_directory=world_library_utf8(legacy);
    }
  }
  if(world_library_canceled(cancel,error))return false;
  next.id=world_library_id();next.spawn_validated=false;catalog.worlds.push_back(std::move(next));
  if(!write(error)) {catalog.worlds.pop_back();return false;}
  return true;
}
WorldLibrary::WorldLibrary(std::filesystem::path root,std::filesystem::path bundle):state_(std::make_unique<State>()) {
  state_->root=std::move(root);state_->bundle=std::move(bundle);state_->directory=state_->root/"saves"/"worlds";
}
WorldLibrary::~WorldLibrary()=default;
const std::vector<WorldLibraryEntry>& WorldLibrary::entries() const {return state_->visible;}
bool WorldLibrary::refresh(std::string& error) {
  error.clear();auto& state=*state_;if(!state.read(error))return false;
  auto maps=state.root/"octaryn-client"/"Assets"/"Maps";
  std::error_code ec;
  if(!std::filesystem::is_directory(maps,ec))maps=state.bundle/"Client"/"Assets"/"Maps";
  size_t examined{};
  const auto bounded=[&] {
    if(++examined<=100000)return true;
    error="Built-in world discovery reached its file limit. Add a smaller folder explicitly.";return false;
  };
  auto discover=[&](const std::filesystem::path& directory) {
    if(!std::filesystem::is_regular_file(directory/"map.json",ec)) {
      if(!std::filesystem::is_directory(directory,ec))return true;
      for(const auto& entry:std::filesystem::directory_iterator(directory,ec)) {
        if(!bounded())return false;
        if(!entry.is_regular_file(ec))continue;
        auto extension=entry.path().extension().string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(extension!=".gltf" && extension!=".glb")continue;
        if(!state.register_source(entry.path(),false,error))return false;
      }
      return !ec;
    }
    MapManifest manifest;
    if(!load_map_manifest_from(directory/"map.json",manifest))return true;
    return state.register_source(manifest.glb,true,error);
  };
  if(!discover(maps))return false;
  if(std::filesystem::is_directory(maps,ec))for(const auto& entry:std::filesystem::directory_iterator(maps,ec)) {
    if(!bounded())return false;
    if(entry.is_directory(ec) && !discover(entry.path()))return false;
  }
  state.publish();return true;
}
bool WorldLibrary::add_source(const std::filesystem::path& source,std::string& error,const std::atomic_bool* cancel) {
  error.clear();return state_->read(error) && state_->register_source(source,false,error,cancel);
}
bool WorldLibrary::scan_folder(const std::filesystem::path& folder,std::string& error,const std::atomic_bool* cancel) {
  error.clear();if(!state_->read(error))return false;std::error_code ec;
  if(!std::filesystem::is_directory(folder,ec)) {error="Choose an existing library folder.";return false;}
  size_t examined{},added{};std::string last_error;
  std::set<std::filesystem::path> authored_tiles;
  // Treat authored tile payloads as parts of their parent world.
  std::filesystem::recursive_directory_iterator manifests(folder,std::filesystem::directory_options::skip_permission_denied,ec),end;
  for(;manifests!=end && !ec;manifests.increment(ec)) {
    if(cancel && cancel->load()) {error="World search canceled.";return false;}
    if(++examined>100000) {error="This folder contains too many files. Choose a smaller folder.";return false;}
    if(manifests.depth()>=8)manifests.disable_recursion_pending();
    if(manifests->path().filename()!="map.json" || !manifests->is_regular_file(ec))continue;
    MapManifest manifest;WorldAuthoredTiles tiles;std::string ignored;
    if(!load_map_manifest_from(manifests->path(),manifest) || !manifest.tiled ||
        !world_library_authored_tiles(manifests->path(),tiles,ignored))continue;
    const auto main=std::filesystem::weakly_canonical(manifest.glb,ec);if(ec)break;
    for(const auto& tile:tiles.tile_files) {
      const auto path=std::filesystem::weakly_canonical(manifests->path().parent_path()/world_library_path(tile),ec);
      if(ec)break;
      if(path!=main)authored_tiles.insert(path);
    }
  }
  if(ec) {error="World search could not inspect authored worlds: "+ec.message();return false;}
  examined=0;
  std::filesystem::recursive_directory_iterator iterator(folder,std::filesystem::directory_options::skip_permission_denied,ec);
  for(;iterator!=end && !ec;iterator.increment(ec)) {
    if(cancel && cancel->load()) {error="World search canceled.";return false;}
    if(++examined>100000) {error="This folder contains too many files. Choose a smaller folder.";return false;}
    if(iterator.depth()>=8)iterator.disable_recursion_pending();
    if(!iterator->is_regular_file(ec))continue;
    auto extension=iterator->path().extension().string();
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(extension!=".glb" && extension!=".gltf")continue;
    const auto canonical=std::filesystem::weakly_canonical(iterator->path(),ec);if(ec)break;
    if(authored_tiles.contains(canonical))continue;
    if(state_->register_source(iterator->path(),false,last_error,cancel))++added;
  }
  if(ec) {error="World search could not finish: "+ec.message();return false;}
  if(!added) {error=last_error.empty()?"No GLB or glTF worlds found in this folder.":last_error;return false;}
  state_->publish();return true;
}
bool WorldLibrary::locate(const std::string& id,const std::filesystem::path& source,std::string& error,const std::atomic_bool* cancel) {
  error.clear();if(!state_->read(error))return false;
  auto* record=state_->find(id);if(!record) {error="World no longer exists in this library.";return false;}
  WorldLibraryRecord replacement;
  if(!world_library_describe(source,replacement,error,cancel))return false;
  const auto prior=*record;replacement.id=record->id;replacement.name=record->name;
  replacement.active_save=record->active_save;replacement.save_count=record->save_count;replacement.last_played=record->last_played;
  replacement.legacy_save_directory=record->legacy_save_directory;
  replacement.spawn_validated=false;
  if(world_library_canceled(cancel,error))return false;
  *record=std::move(replacement);if(state_->write(error))return true;*record=prior;return false;
}
}
