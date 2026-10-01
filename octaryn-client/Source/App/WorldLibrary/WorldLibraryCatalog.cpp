#include "WorldLibraryRecords.h"
#include "SessionFiles.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cmath>
#include <random>
#include <set>

namespace octaryn::client::app {
std::string world_library_utf8(const std::filesystem::path& path) {
  const auto value=path.u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};
}
std::filesystem::path world_library_path(const std::string& path) {
  return std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str()));
}
std::string world_library_id() {
  std::random_device random;std::string id;constexpr char digits[]="0123456789abcdef";
  for(unsigned index=0;index<16;++index) {const auto byte=random()&255;id+=digits[byte>>4];id+=digits[byte&15];}
  return id;
}
bool world_library_valid_id(const std::string& id) {
  return id.size()==32 && std::all_of(id.begin(),id.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');});
}
bool world_library_load_catalog(const std::filesystem::path& path,WorldLibraryCatalog& catalog,std::string& error) {
  std::string text;
  if(!local_session::read_text(path,text,4u*1024*1024)) {error="World library could not be read.";return false;}
  WorldLibraryCatalog next;
  constexpr glz::opts options{.error_on_unknown_keys=true,.error_on_missing_keys=true};
  if(glz::read<options>(next,text) || next.version!=1 || next.worlds.size()>4096) {error="World library is invalid.";return false;}
  std::set<std::string> ids;
  for(const auto& world:next.worlds) {
    bool valid=world_library_valid_id(world.id) && ids.insert(world.id).second && !world.source.empty() &&
        !world.name.empty() && world.name.size()<=256 && world.save_count<=100000 &&
        (world.active_save.empty() || world_library_valid_id(world.active_save)) &&
        world.resources.size()<=4096 && world_library_path(world.source).is_absolute() &&
        (world.manifest.empty() || world_library_path(world.manifest).is_absolute());
    for(float coordinate:world.spawn)valid=valid && std::isfinite(coordinate);
    for(const auto& resource:world.resources)valid=valid && !resource.path.empty() && resource.digest.size()==64 && world_library_path(resource.path).is_absolute();
    if(!valid || !std::isfinite(world.yaw) || !std::isfinite(world.pitch)) {error="World library contains invalid metadata.";return false;}
  }
  catalog=std::move(next);return true;
}
bool WorldLibrary::State::read(std::string& error) {
  if(initialized)return true;
  std::error_code ec;
  if(std::filesystem::exists(directory/"catalog.json",ec) && !world_library_load_catalog(directory/"catalog.json",catalog,error))return false;
  if(ec) {error="World library storage is unavailable.";return false;}
  initialized=true;return true;
}
bool WorldLibrary::State::write(std::string& error) {
  std::error_code ec;std::filesystem::create_directories(directory,ec);
  std::string text;
  if(ec || glz::write_json(catalog,text) || text.size()>4u*1024*1024 || !local_session::write_text(directory/"catalog.json",text)) {
    error="World library could not be saved.";return false;
  }
  publish();return true;
}
WorldLibraryRecord* WorldLibrary::State::find(const std::string& id) {
  const auto found=std::find_if(catalog.worlds.begin(),catalog.worlds.end(),[&](const auto& world){return world.id==id;});
  return found==catalog.worlds.end()?nullptr:&*found;
}
void WorldLibrary::State::publish() {
  visible.clear();
  for(const auto& world:catalog.worlds) {
    std::error_code ec;
    const bool available=std::filesystem::is_regular_file(world_library_path(world.source),ec) &&
        (world.manifest.empty() || std::filesystem::is_regular_file(world_library_path(world.manifest),ec));
    WorldLibraryEntry entry{world.id,world.name,world.source,world.format,"New world",world.last_played,available,{},world.active_save};
    entry.preparation_required=available && world.preparation_required.value_or(false);
    if(entry.preparation_required) {entry.available=false;entry.save_label="Preparation needed";}
    const auto saves=directory/world.id;
    if(std::filesystem::is_directory(saves,ec))for(const auto& save:std::filesystem::directory_iterator(saves,ec)) {
      if(!save.is_directory(ec) || !world_library_valid_id(save.path().filename().string()))continue;
      WorldSaveDescriptor descriptor;std::string ignored;
      if(!world_library_read_save(save.path(),descriptor,ignored) || descriptor.world_id!=world.id)continue;
      entry.saves.push_back({descriptor.save_id,descriptor.name,descriptor.last_played});
      if(descriptor.save_id==world.active_save)entry.save_label=descriptor.name;
    }
    std::sort(entry.saves.begin(),entry.saves.end(),[](const auto& a,const auto& b){return a.last_played>b.last_played;});
    visible.push_back(std::move(entry));
  }
}
}
