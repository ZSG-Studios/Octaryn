#pragma once
#include "MapManifest.h"
#include "SceneTransitionHost.h"
#include <array>
#include <fstream>
#include <glaze/glaze.hpp>
#include <stdexcept>

namespace octaryn::client::app {
struct TransitionSceneDescriptor {unsigned version{};bool prepared{};std::string scene,map_manifest;};
struct TransitionServerManifest {
  unsigned version=1;
  std::string map="scene.gltf";
  std::array<float,3> spawn{};
  float yaw{},pitch{};
};
inline void write_scene_spawn_manifest(MapManifest& manifest,const std::filesystem::path& runtime_root,
    const std::string& name) {
  namespace fs=std::filesystem;
  const auto directory=runtime_root/"scene-transitions";
  fs::create_directories(directory);
  manifest.authority_spawn_manifest=directory/(name+".json");
  // Streaming resources stay relative to their immutable source manifest.
  if(!manifest.tiled && manifest.scene_catalog.empty())manifest.manifest=manifest.authority_spawn_manifest;
  TransitionServerManifest server;
  server.spawn={manifest.spawn_x,manifest.spawn_y,manifest.spawn_z};server.yaw=manifest.yaw;server.pitch=manifest.pitch;
  std::string encoded;
  if(glz::write_json(server,encoded))throw std::runtime_error("Scene transition spawn serialization failed");
  std::ofstream output(manifest.authority_spawn_manifest,std::ios::binary|std::ios::trunc);
  if(!output.write(encoded.data(),static_cast<std::streamsize>(encoded.size())))
    throw std::runtime_error("Scene transition spawn manifest write failed");
}
// The managed owner verifies the declared closed scene before queuing. This
// boundary resolves only a relative payload inside that verified scene folder.
inline MapManifest resolve_scene_transition(const host::SceneTransitionRequest& request,
    const std::filesystem::path& runtime_root,bool source_spawn=false) {
  namespace fs=std::filesystem;
  if(!request.revision || !host::transition_pose_valid(request.pose))
    throw std::runtime_error("Scene transition pose or revision invalid");
  const auto descriptor=fs::canonical(request.descriptor_path);
  const auto size=fs::file_size(descriptor);
  if(!size || size>4*1024*1024)throw std::runtime_error("Scene transition descriptor exceeds admission");
  std::ifstream file(descriptor,std::ios::binary);
  std::string text(static_cast<std::size_t>(size),'\0');
  if(!file.read(text.data(),static_cast<std::streamsize>(size)))throw std::runtime_error("Scene transition descriptor unreadable");
  TransitionSceneDescriptor value;
  constexpr glz::opts opts{.error_on_unknown_keys=false};
  if(glz::read<opts>(value,text) || value.version!=1 || !value.prepared || value.scene.empty() ||
      value.scene.find_first_of(":\\")!=std::string::npos || value.scene.find('\0')!=std::string::npos)
    throw std::runtime_error("Scene transition requires a prepared scene descriptor");
  const auto confined_source=[&](const std::string& resource) {
    if(resource.empty() || resource.find_first_of(":\\")!=std::string::npos || resource.find('\0')!=std::string::npos)
      throw std::runtime_error("Scene transition resource path invalid");
    const auto relative=fs::u8path(resource);
    if(relative.is_absolute() || relative.has_root_name())throw std::runtime_error("Scene transition resource must be relative");
    for(const auto& part:relative)if(part==".." || part==".")throw std::runtime_error("Scene transition resource escapes package");
    const auto source=fs::canonical(descriptor.parent_path()/relative);
    const auto confined=source.lexically_relative(descriptor.parent_path());
    if(confined.empty() || confined.is_absolute())throw std::runtime_error("Scene transition resource escapes package");
    for(const auto& part:confined)if(part=="..")throw std::runtime_error("Scene transition link escapes package");
    if(!fs::is_regular_file(source))throw std::runtime_error("Scene transition resource unavailable");
    return source;
  };
  const auto source=confined_source(value.scene);
  if(source.extension()!=".gltf")throw std::runtime_error("Scene transition payload unavailable");
  MapManifest result;
  if(!value.map_manifest.empty()) {
    const auto payload=confined_source(value.map_manifest);
    if(payload.extension()!=".json" || !load_map_manifest_from(payload,result))
      throw std::runtime_error("Scene transition map manifest invalid");
    const auto fallback=fs::canonical(result.glb).lexically_relative(descriptor.parent_path());
    if(fallback.empty() || fallback.is_absolute())throw std::runtime_error("Scene transition map payload escapes package");
    for(const auto& part:fallback)if(part=="..")throw std::runtime_error("Scene transition map payload escapes package");
  } else result.glb=source;
  result.scene_asset=request.asset_id;result.scene_descriptor=descriptor;result.replace_scene=true;
  if(!source_spawn) {
    result.spawn_x=static_cast<float>(request.pose.x);result.spawn_y=static_cast<float>(request.pose.y);
    result.spawn_z=static_cast<float>(request.pose.z);result.yaw=request.pose.yaw;result.pitch=request.pose.pitch;
  }
  write_scene_spawn_manifest(result,runtime_root,std::to_string(request.revision));
  return result;
}
}
