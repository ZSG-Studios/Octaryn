#include "WorldLibraryRecords.h"
#include "WorldLibraryIo.h"
#include "MapManifest.h"
#include "SessionFiles.h"
#include "ScenePreparationWorld.h"
#include "SceneCatalog.h"
#include "ResourceDigest.h"
#include "FilePath.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <optional>
#include <regex>
#include <stdexcept>

namespace octaryn::client::app {
struct PreparedWorld {
  int version{1};
  std::string world_id,source,fingerprint,catalog,catalog_digest;
  std::array<float,3> spawn{};
  float yaw{},pitch{};
  std::optional<std::string> hierarchy,hierarchy_digest;
};
struct PreparedManifest {
  int version{1};
  std::string map,scene_catalog;
  std::array<float,3> spawn{};
  float yaw{},pitch{};
  std::optional<std::string> scene_hierarchy;
};
namespace {
namespace geometry=rendering::virtual_geometry;
std::string fingerprint(const WorldLibraryRecord& world) {
  std::string identity="world-source-v1:";
  for(const auto& resource:world.resources)
    identity+=resource.path+":"+std::to_string(resource.bytes)+":"+resource.digest+"\n";
  return content::resource_digest({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()});
}
std::filesystem::path metadata(const std::filesystem::path& directory,const std::string& id) {
  return directory/"prepared"/id/"prepared.json";
}
void view_hint(const WorldLibraryRecord& world,std::array<float,3>& camera,float& yaw,float& pitch) {
  camera=world.spawn;yaw=world.yaw;pitch=world.pitch;
  if(!world.manifest.empty())return;
  auto config=world_library_path(world.source);config.replace_extension(".cfg");
  std::error_code ec;
  if(std::filesystem::file_size(config,ec)>65536 || ec)return;
  std::ifstream input(config);std::string line;
  const std::regex number(R"([-+]?(?:\d*\.)?\d+(?:[eE][-+]?\d+)?)");
  while(std::getline(input,line))if(line.starts_with("--camerastring ")) {
    const auto start=line.find('{');if(start==std::string::npos)return;
    line=line.substr(start);std::array<float,6> values{};unsigned count{};
    for(auto match=std::sregex_iterator(line.begin(),line.end(),number);match!=std::sregex_iterator() && count<6;++match)
      values[count++]=std::stof(match->str());
    if(count!=6 || !std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);} ))return;
    std::copy_n(values.begin(),3,camera.begin());
    const float dx=values[3]-values[0],dy=values[4]-values[1],dz=values[5]-values[2];
    yaw=std::atan2(dx,-dz);pitch=std::atan2(dy,std::hypot(dx,dz));return;
  }
}
bool preparation_failure(const geometry::ScenePreparationRequest& request,const geometry::ScenePreparationResult& result,std::string& error) {
  std::fprintf(stderr,"world_library_preparation_failed render_bytes=%llu collision_bytes=%llu reason=%s\n",
      static_cast<unsigned long long>(result.render_bytes),static_cast<unsigned long long>(result.collision_bytes),error.c_str());
  if(!result.canceled && (result.render_bytes>request.gpu_budget_bytes || result.collision_bytes>request.collision_budget_bytes))
    error="This world exceeds the current streaming limit. Its geometry needs more efficient spatial and detail preparation before it can open.";
  return false;
}
}
bool world_library_prepared_scene(const std::filesystem::path& directory,const WorldLibraryRecord& world,MapManifest& output) {
  try {
    if(!world_library_valid_id(world.id))return false;
    std::string text,error;PreparedWorld prepared;
    constexpr glz::opts options{.error_on_unknown_keys=true,.error_on_missing_keys=true};
    const auto file=metadata(directory,world.id);
    if(!local_session::read_text(file,text,65536) || glz::read<options>(prepared,text) || (prepared.version!=1 && prepared.version!=2) ||
        prepared.world_id!=world.id || prepared.source!=world.source || prepared.fingerprint!=fingerprint(world))return false;
    const auto catalog=world_library_path(prepared.catalog);
    world_library_note_io(WorldLibraryIo::PreparedCatalog);
    if(!catalog.is_absolute() || prepared.catalog_digest.size()!=64 ||
        content::resource_file_digest(catalog,error,32ull*1024*1024)!=prepared.catalog_digest)return false;
    geometry::SceneCatalog scene;
    std::error_code ec;
    if(!geometry::read_scene_catalog(catalog,scene,error) ||
        !std::filesystem::equivalent(world_library_path(scene.source),world_library_path(world.source),ec) || ec)return false;
    std::filesystem::path hierarchy;
    if(prepared.version==2) {
      if(!prepared.hierarchy || !prepared.hierarchy_digest || prepared.hierarchy_digest->size()!=64)return false;
      hierarchy=world_library_path(*prepared.hierarchy);
      if(!hierarchy.is_absolute() || hierarchy.lexically_normal()!=(catalog.parent_path()/"hierarchy"/"scene.json").lexically_normal() ||
          content::canonical_file_path(hierarchy)!=(content::canonical_file_path(catalog.parent_path())/"hierarchy"/"scene.json") ||
          content::resource_file_digest(hierarchy,error,32ull*1024*1024)!=*prepared.hierarchy_digest ||
          !geometry::validate_scene_world_hierarchy(catalog,hierarchy,error))return false;
    }else if(prepared.hierarchy || prepared.hierarchy_digest)return false;
    for(const auto value:prepared.spawn)if(!std::isfinite(value))return false;
    PreparedManifest manifest;
    if(!std::isfinite(prepared.yaw) || !std::isfinite(prepared.pitch) ||
        !local_session::read_text(file.parent_path()/"map.json",text,65536) || glz::read<options>(manifest,text) ||
        manifest.version!=1 || manifest.map!=world_library_utf8(world_library_path(world.source).filename()) ||
        manifest.scene_catalog!=prepared.catalog || manifest.spawn!=prepared.spawn ||
        manifest.yaw!=prepared.yaw || manifest.pitch!=prepared.pitch || manifest.scene_hierarchy!=prepared.hierarchy)return false;
    output.glb=world_library_path(world.source);output.manifest=file.parent_path()/"map.json";
    output.scene_catalog=catalog;
    output.scene_hierarchy=hierarchy;
    output.spawn_x=prepared.spawn[0];output.spawn_y=prepared.spawn[1];output.spawn_z=prepared.spawn[2];
    output.yaw=prepared.yaw;output.pitch=prepared.pitch;return true;
  }catch(const std::exception&) {return false;}
}
bool world_library_needs_preparation(const std::filesystem::path& directory,const WorldLibraryRecord& world) {
  if(!world_library_preparation_required(world_library_path(world.source)))return false;
  MapManifest prepared;return !world_library_prepared_scene(directory,world,prepared);
}
bool WorldLibrary::prepare(const std::string& id,std::string& error,const std::atomic_bool* cancel,
    std::function<void(const std::string&)> progress) {
  try {
    error.clear();if(!state_->read(error))return false;
    auto* world=state_->find(id);
    if(!world) {error="World no longer exists in this library.";return false;}
    if(!state_->validate_source(*world,error,cancel,progress,true))return false;
    const auto source=world_library_path(world->source);
    std::error_code ec;
    const auto cache_root=std::filesystem::is_directory(state_->root/".git",ec)
        ?state_->bundle.parent_path()/"caches"/"scenes":state_->root/"geometry-cache"/"scenes";
    const auto cache=cache_root/id/fingerprint(*world);
    geometry::ScenePreparationRequest request;request.source=source;request.catalog=std::filesystem::absolute(cache/"scene.json");
    float yaw{},pitch{};view_hint(*world,request.camera,yaw,pitch);request.actor=request.camera;
    const auto notify=[&](const geometry::ScenePreparationProgress& event) {
      if(!progress)return;
      if(event.stage==geometry::ScenePreparationStage::Spawn)progress("Checking the starting position...");
      else if(event.stage==geometry::ScenePreparationStage::Layout)
        progress(event.requested?"Organizing geometry: "+std::to_string(event.completed)+" / "+std::to_string(event.requested):"Organizing geometry...");
      else if(event.stage==geometry::ScenePreparationStage::Bounds && event.requested)
        progress("Checking collision bounds: "+std::to_string(event.completed)+" / "+std::to_string(event.requested));
      else if(event.requested)progress("Preparing world: "+std::to_string(event.completed)+" / "+std::to_string(event.requested));
      else progress("Inspecting world files...");
    };
    geometry::ScenePreparationWorldResult result;
    if(!geometry::prepare_scene_world(request,result,error,cancel,notify) || !result.scene.neighborhood_ready || !result.hierarchy_ready)
      return preparation_failure(request,result.scene,error);
    const auto spawn=result.spawn;
    PreparedWorld prepared{2,id,world->source,fingerprint(*world),world_library_utf8(result.catalog),
        content::resource_file_digest(result.catalog,error,32ull*1024*1024),spawn,yaw,pitch};
    if(prepared.catalog_digest.empty())return false;
    prepared.hierarchy=world_library_utf8(result.hierarchy);
    prepared.hierarchy_digest=content::resource_file_digest(result.hierarchy,error,32ull*1024*1024);
    if(prepared.hierarchy_digest->empty())return false;
    if(world_library_canceled(cancel,error))return false;
    const auto path=metadata(state_->directory,id);std::filesystem::create_directories(path.parent_path());
    std::string text;
    const PreparedManifest manifest{1,world_library_utf8(source.filename()),prepared.catalog,spawn,yaw,pitch,prepared.hierarchy};
    if(glz::write_json(manifest,text) || !local_session::write_text(path.parent_path()/"map.json",text) ||
        glz::write_json(prepared,text) || !local_session::write_text(path,text)) {
      error="Prepared world metadata could not be saved.";return false;
    }
    const auto previous=*world;world->preparation_required=false;world->spawn_validated=true;
    if(!state_->write(error)) {*world=previous;return false;}
    std::printf("world_library_prepared id=%s neighborhood_ready=1 full_scene_ready=%u hierarchy_ready=%u saves_created=0\n",
        id.c_str(),unsigned(result.scene.full_scene_ready),unsigned(result.hierarchy_ready));
    return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
