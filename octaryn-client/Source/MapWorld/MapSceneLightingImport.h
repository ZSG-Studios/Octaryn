#pragma once
#include "MapSceneLighting.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include <simdjson.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#include <cmath>
#include <stdexcept>

namespace octaryn::client::rendering {
inline MapSceneEnvironment read_map_scene_environment(simdjson::dom::element value) {
  const auto require=[](bool condition,const char* reason) {if(!condition)throw std::runtime_error(reason);};
  simdjson::dom::object object;
  require(value.get_object().get(object)==simdjson::SUCCESS,"scene environment must be an object");
  for(const auto field:object)require(field.key=="version" || field.key=="sky_enabled" ||
      field.key=="ambient" || field.key=="directional_color" || field.key=="directional_direction" ||
      field.key=="background","unknown scene environment field");
  std::uint64_t version{};
  require(object["version"].get_uint64().get(version)==simdjson::SUCCESS && version==1,"unsupported scene environment version");
  MapSceneEnvironment result;result.enabled=true;
  require(object["sky_enabled"].get_bool().get(result.sky_enabled)==simdjson::SUCCESS,"scene sky flag missing or invalid");
  const auto vector=[&](const char* name,std::array<float,3>& destination,bool signed_values=false) {
    simdjson::dom::array values;
    require(object[name].get_array().get(values)==simdjson::SUCCESS && values.size()==3,"scene environment requires three components");
    unsigned index{};
    for(const auto component:values) {
      double number{};require(component.get_double().get(number)==simdjson::SUCCESS,"scene environment component not numeric");
      require(std::isfinite(number) && std::abs(number)<=100000 && (signed_values || number>=0),"scene environment component out of bounds");
      destination[index++]=float(number);
    }
  };
  vector("ambient",result.ambient);vector("directional_color",result.directional_color);
  vector("background",result.background);vector("directional_direction",result.directional_direction,true);
  double length{};for(const auto component:result.directional_direction)length+=double(component)*component;
  require(std::abs(length-1)<=.001,"scene directional vector must be normalized");
  return result;
}
}
