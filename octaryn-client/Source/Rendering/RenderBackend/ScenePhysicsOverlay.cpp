#include "WorldRendererInternal.h"
#include "../../VirtualGeometry/SceneCatalog.h"
#include "../../VirtualGeometry/ScenePreparation.h"
#include "FilePath.h"
#include "ResourceDigest.h"
#include <SDL3/SDL.h>
#include <glaze/glaze.hpp>
#include <fstream>
#include <optional>

namespace octaryn::client::rendering {
struct PhysicsOverlayManifest {std::string map;};
struct PhysicsOverlaySource {int version{};std::optional<std::string> presentationPath;};
namespace {
template<class T> bool read(const std::filesystem::path& path,T& out,std::uint64_t limit) {
  std::error_code ec;const auto bytes=std::filesystem::file_size(path,ec);
  if(ec || !bytes || bytes>limit)return false;
  std::ifstream input(path,std::ios::binary);std::string text(std::size_t(bytes),'\0');
  if(!input.read(text.data(),std::streamsize(bytes)))return false;
  constexpr glz::opts options{.error_on_unknown_keys=false};
  return !glz::read<options>(out,text);
}
bool prepared_catalog(WorldRenderer& renderer,const std::filesystem::path& source,std::filesystem::path& cache) {
  world_renderer_load_stage(renderer,"Preparing source object instances",true);
  virtual_geometry::SceneCatalog imported;std::string error;
  if(!virtual_geometry::import_scene_catalog(source,imported,error)) {renderer.status=error;return false;}
  char* preference=SDL_GetPrefPath("ZSGStudios","Octaryn");
  if(!preference) {renderer.status="physics presentation cache unavailable";return false;}
  const auto directory=std::filesystem::u8path(preference);SDL_free(preference);
  const auto identity=imported.source+":"+imported.source_hash;
  const auto hash=content::resource_digest({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()});
  cache=directory/"geometry-cache"/"scene-physics"/hash/"scene.json";
  virtual_geometry::SceneCatalog previous;
  const bool cached=virtual_geometry::read_scene_catalog(cache,previous,error) && previous.source_hash==imported.source_hash;
  if(!cached && !virtual_geometry::write_scene_catalog(cache,imported,error)) {renderer.status=error;return false;}
  virtual_geometry::ScenePreparationResult prepared;
  if(!virtual_geometry::prepare_scene_range(cache,0,imported.parts.size(),virtual_geometry::ScenePreparationMode::Bounds,prepared,error) ||
     !virtual_geometry::prepare_scene_range(cache,0,imported.parts.size(),virtual_geometry::ScenePreparationMode::Geometry,prepared,error)) {
    renderer.status=error;return false;
  }
  return true;
}
}
bool load_scene_physics_overlay(WorldRenderer& renderer,const std::filesystem::path& manifest) {
  PhysicsOverlayManifest map;
  if(!read(manifest,map,4*1024*1024)) {renderer.status="physics presentation map source unavailable";return false;}
  const auto authority=content::canonical_file_path(manifest.parent_path()/std::filesystem::u8path(map.map));
  auto sidecar=authority;sidecar.replace_extension(".physics.json");
  std::error_code ec;if(!std::filesystem::exists(sidecar,ec))return !ec;
  PhysicsOverlaySource physics;
  if(!read(sidecar,physics,16*1024*1024) || physics.version!=1) {renderer.status="physics presentation catalogue invalid";return false;}
  if(!physics.presentationPath)return true;
  const auto relative=std::filesystem::u8path(*physics.presentationPath);
  if(relative.empty() || relative.is_absolute()) {renderer.status="physics presentation path invalid";return false;}
  for(const auto& part:relative)if(part=="..") {renderer.status="physics presentation escapes source directory";return false;}
  const auto source=content::canonical_file_path(authority.parent_path()/relative);
  std::filesystem::path cache;if(!prepared_catalog(renderer,source,cache))return false;
  world_renderer_load_stage(renderer,"Loading movable source objects");
  auto session=std::make_unique<SceneSession>();
  if(!session->load(renderer,cache,source,512ull*1024*1024,true)) {renderer.status=session->error();return false;}
  renderer.scene_physics_source=authority.generic_string();renderer.scene_session=std::move(session);
  std::printf("scene_physics_overlay initialized=1 source=%s authority=server\n",renderer.scene_physics_source.c_str());
  std::fflush(stdout);return true;
}
bool load_scene_physics_map(WorldRenderer& renderer,const std::filesystem::path& source,bool& handled) {
  handled=false;auto sidecar=source;sidecar.replace_extension(".physics.json");
  std::error_code ec;if(!std::filesystem::exists(sidecar,ec))return !ec;
  PhysicsOverlaySource physics;
  if(!read(sidecar,physics,16*1024*1024) || physics.version!=1) {renderer.status="source body catalogue invalid";return false;}
  handled=true;std::filesystem::path catalog;if(!prepared_catalog(renderer,source,catalog))return false;
  world_renderer_load_stage(renderer,"Loading source object instances");
  auto session=std::make_unique<SceneSession>();
  if(!session->load(renderer,catalog,source)) {renderer.status=session->error();return false;}
  renderer.scene_session=std::move(session);renderer.status="scene_loading";
  std::printf("scene_physics_map initialized=1 source=%s original_instances=1 authority=server\n",source.generic_string().c_str());
  std::fflush(stdout);return true;
}
}
