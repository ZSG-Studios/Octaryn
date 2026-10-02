#include "SceneCollisionBodies.h"
#include "SceneCollisionCatalog.h"
#include "FilePath.h"
#include <glaze/glaze.hpp>
#include <fstream>
#include <set>
#include <algorithm>

namespace octaryn::character_motion {
struct ExcludedBody {std::uint64_t sourceId{};std::string nodeName;};
struct ExcludedBodies {unsigned version{};std::vector<ExcludedBody> bodies;};
struct ExcludedInstance {unsigned node{};std::string name;};
struct ExcludedCatalog {std::vector<ExcludedInstance> instances;};
constexpr glz::opts options{.error_on_unknown_keys=false};
bool read_scene_body_exclusions(const std::filesystem::path& path,std::vector<std::string>& names,std::string& error) {
  names.clear();std::error_code ec;const auto size=std::filesystem::file_size(content::file_io_path(path),ec);
  if(ec)return true;
  if(!size || size>16*1024*1024) {error="scene body catalog exceeds limits";return false;}
  std::ifstream input(content::file_io_path(path),std::ios::binary);std::string text(size,'\0');ExcludedBodies bodies;
  if(!input.read(text.data(),std::streamsize(size)) || glz::read<options>(bodies,text) || bodies.version!=1) {
    error="scene body exclusion catalog is invalid";return false;
  }
  std::set<std::uint64_t> ids;
  for(const auto& body:bodies.bodies) {
    if(!body.sourceId || body.nodeName.empty() || !ids.insert(body.sourceId).second) {error="scene body exclusion identities are invalid";return false;}
    names.push_back(body.nodeName);
  }
  std::sort(names.begin(),names.end());
  if(std::adjacent_find(names.begin(),names.end())!=names.end()) {error="scene body exclusion names are duplicated";return false;}
  return true;
}
bool exclude_scene_bodies(const std::filesystem::path& source,const std::string& text,
    SceneCollisionCatalog& scene,std::string& error) {
  auto path=source;path.replace_extension(".physics.json");std::error_code ec;
  const auto size=std::filesystem::file_size(content::file_io_path(path),ec);
  if(ec)return true;if(!size || size>16*1024*1024) {error="scene body catalog exceeds limits";return false;}
  std::ifstream input(content::file_io_path(path),std::ios::binary);std::string content(size,'\0');
  ExcludedBodies bodies;ExcludedCatalog catalog;
  if(!input.read(content.data(),std::streamsize(size)) || glz::read<options>(bodies,content) || bodies.version!=1 ||
      glz::read<options>(catalog,text)) {error="scene body collision exclusion is invalid";return false;}
  std::set<std::string> names;std::set<std::uint64_t> ids;
  for(const auto& body:bodies.bodies)if(!body.sourceId || body.nodeName.empty() || !names.insert(body.nodeName).second || !ids.insert(body.sourceId).second) {
    error="scene body collision identities are invalid";return false;
  }
  std::set<unsigned> nodes;for(const auto& instance:catalog.instances)if(names.contains(instance.name))nodes.insert(instance.node);
  std::erase_if(scene.instances,[&](const auto& instance){return nodes.contains(instance.node);});return true;
}
}
