#pragma once
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <glaze/glaze.hpp>

namespace octaryn::client::app {
struct GameplayRoutePhase {
  std::string name;
  double seconds{};
  int forward{},strafe{};
  bool sprint{};
  std::optional<float> yaw,pitch;
  float turn{};
  std::optional<std::array<float,2>> target;
  float tolerance{.5f},min_distance{};
};
struct GameplayRouteDefinition {
  int version{};
  std::vector<GameplayRoutePhase> phases;
};

inline GameplayRouteDefinition load_gameplay_route(const std::filesystem::path& path) {
  const auto size=std::filesystem::file_size(path);
  if(!size || size>65536)throw std::runtime_error("Gameplay route file must be 1..65536 bytes");
  std::ifstream file(path,std::ios::binary);
  std::string text(size,'\0');
  if(!file.read(text.data(),std::streamsize(size)))throw std::runtime_error("Cannot read gameplay route");
  GameplayRouteDefinition result;
  if(glz::read_json(result,text) || result.version!=1 || result.phases.empty() || result.phases.size()>256)
    throw std::runtime_error("Invalid gameplay route schema");
  double total=0;
  for(const auto& phase:result.phases) {
    if(phase.name.empty() || phase.name.size()>32 ||
        phase.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos ||
        !std::isfinite(phase.seconds) || phase.seconds<=0 || phase.seconds>3600 ||
        phase.forward < -1 || phase.forward > 1 || phase.strafe < -1 || phase.strafe > 1 ||
        !std::isfinite(phase.turn) || std::abs(phase.turn)>6.3f ||
        (phase.yaw && !std::isfinite(*phase.yaw)) ||
        (phase.pitch && (!std::isfinite(*phase.pitch) || std::abs(*phase.pitch)>1.5f)) ||
        !std::isfinite(phase.min_distance) || phase.min_distance<0 ||
        !std::isfinite(phase.tolerance) || phase.tolerance<=0 || phase.tolerance>10 ||
        (phase.target && (!std::isfinite((*phase.target)[0]) || !std::isfinite((*phase.target)[1]))))
      throw std::runtime_error("Invalid gameplay route phase");
    if((phase.forward || phase.strafe) && !phase.target && phase.min_distance<=0)
      throw std::runtime_error("Moving gameplay phases require target or positive min_distance");
    if(phase.target && (phase.forward!=1 || phase.strafe || phase.turn!=0))
      throw std::runtime_error("Waypoint phases require forward=1, strafe=0 and turn=0");
    total+=phase.seconds;
  }
  if(total>86400)throw std::runtime_error("Gameplay route exceeds 24-hour bound");
  return result;
}
}
