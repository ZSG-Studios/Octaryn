#pragma once
#include "MapManifest.h"
#include <glaze/glaze.hpp>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string_view>

namespace octaryn::server::map_world {
struct MapTransferPose {
  unsigned version{};
  std::string map;
  std::array<float,3> spawn{};
  float yaw{},pitch{};
};
inline bool apply_map_transfer_spawn(MapManifest& manifest) {
  const auto* policy=std::getenv("OCTARYN_SERVER_MAP_TRANSFER_SPAWN");
  const auto* source=std::getenv("OCTARYN_SERVER_MAP_TRANSFER_POSE_PATH");
  if(!source || !*source)return true;
  if(!policy || std::string_view(policy)!="1")return false;
  try {
    const auto path=std::filesystem::u8path(source);
    if(!path.is_absolute() || !std::filesystem::is_regular_file(path))return false;
    const auto size=std::filesystem::file_size(path);if(!size || size>4096)return false;
    std::ifstream file(path,std::ios::binary);std::string text(static_cast<size_t>(size),'\0');
    if(!file.read(text.data(),static_cast<std::streamsize>(size)))return false;
    MapTransferPose pose;
    constexpr glz::opts options{.error_on_unknown_keys=true,.error_on_missing_keys=true};
    if(glz::read<options>(pose,text) || pose.version!=1 || pose.map.empty() ||
        !std::isfinite(pose.yaw) || std::abs(pose.yaw)>1000000 || !std::isfinite(pose.pitch) || std::abs(pose.pitch)>1.55f)return false;
    for(float value:pose.spawn)if(!std::isfinite(value) || std::abs(value)>1000000)return false;
    manifest.spawn_x=pose.spawn[0];manifest.spawn_y=pose.spawn[1];manifest.spawn_z=pose.spawn[2];
    manifest.yaw=pose.yaw;manifest.pitch=pose.pitch;return true;
  }catch(...){return false;}
}
}
