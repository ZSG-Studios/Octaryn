#pragma once
#include <fastgltf/core.hpp>
#include "MapSceneLightingImport.h"
#include "GltfCollisionImport.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include <simdjson.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>

namespace octaryn::client::rendering {
inline constexpr unsigned map_layer_limit=8;
struct MapLayerSource {
  std::uint32_t texture{},texcoord{};
  std::int32_t normal_texture{-1};
  std::array<float,6> transform{1,0,0,0,1,0};
};
struct MapLayerSourceMaterial {std::vector<MapLayerSource> layers;};
// Retains only the declared material contract during the existing parser pass.
class MapLayerImport {
  std::map<std::size_t,MapLayerSourceMaterial> materials_;
  std::set<std::size_t> zero_basis_;
  struct Forward {bool additive{},view_fade{};std::array<float,4> parameters{0,1,1,1};};
  std::map<std::size_t,Forward> forward_;
  std::map<std::size_t,MapSceneEnvironment> environments_;
  std::string error_;
  octaryn::assets::GltfCollisionImport collision_;
  static void require(bool value,const char* reason) {
    if(!value)throw std::runtime_error(reason);
  }
  static simdjson::dom::element field(simdjson::dom::object object,const char* name) {
    simdjson::dom::element value;
    require(object[name].get(value)==simdjson::SUCCESS,"layer material field missing or invalid");return value;
  }
  static std::uint32_t number(simdjson::dom::element value) {
    std::uint64_t result{};
    require(value.get_uint64().get(result)==simdjson::SUCCESS && result<=UINT32_MAX,
        "layer material index must be unsigned32");return std::uint32_t(result);
  }
  static bool optional(simdjson::dom::object object,const char* name,simdjson::dom::element& value) {
    const auto error=object[name].get(value);
    if(error==simdjson::NO_SUCH_FIELD)return false;
    require(error==simdjson::SUCCESS,"layer material optional field invalid");return true;
  }
  static MapLayerSourceMaterial read(simdjson::dom::element value) {
    simdjson::dom::object object;
    require(value.get_object().get(object)==simdjson::SUCCESS,"layer material must be an object");
    for(auto item:object)require(item.key=="version" || item.key=="layers","unknown layer material field");
    require(number(field(object,"version"))==1,"unsupported layer material version");
    simdjson::dom::array array;
    require(field(object,"layers").get_array().get(array)==simdjson::SUCCESS &&
        array.size()>0 && array.size()<=map_layer_limit,"layer material requires 1 to 8 ordered layers");
    MapLayerSourceMaterial result;
    for(auto element:array) {
      simdjson::dom::object layer;
      require(element.get_object().get(layer)==simdjson::SUCCESS,"layer must be an object");
      for(auto item:layer)require(item.key=="texture" || item.key=="normal_texture" ||
          item.key=="texcoord" || item.key=="transform","unknown texture layer field");
      MapLayerSource source;source.texture=number(field(layer,"texture"));
      simdjson::dom::element optional_value;
      if(optional(layer,"normal_texture",optional_value)) {
        const auto index=number(optional_value);require(index<=INT32_MAX,"layer normal texture index too large");
        source.normal_texture=std::int32_t(index);
      }
      if(optional(layer,"texcoord",optional_value))source.texcoord=number(optional_value);
      require(source.texcoord<=1,"layers require TEXCOORD_0 or TEXCOORD_1");
      if(optional(layer,"transform",optional_value)) {
        simdjson::dom::array transform;
        require(optional_value.get_array().get(transform)==simdjson::SUCCESS && transform.size()==6,
            "layer UV transform requires six floats");
        unsigned index=0;
        for(auto component:transform) {
          double number{};require(component.get_double().get(number)==simdjson::SUCCESS,"layer UV transform not numeric");
          const auto converted=float(number);require(std::isfinite(converted),"layer UV transform nonfinite");
          source.transform[index++]=converted;
        }
      }
      if(!result.layers.empty())require(source.texcoord==result.layers.front().texcoord &&
          source.transform==result.layers.front().transform,"layer normal frames must share the same authored UV basis");
      result.layers.push_back(source);
    }
    return result;
  }
  static void extras(simdjson::dom::object* object,std::size_t index,fastgltf::Category category,void* pointer) {
    auto& self=*static_cast<MapLayerImport*>(pointer);
    self.collision_.extras(object,index,category);
    if(category!=fastgltf::Category::Materials && category!=fastgltf::Category::Scenes)return;
    if(!self.error_.empty())return;
    try {
      simdjson::dom::element value;
      if(category==fastgltf::Category::Scenes) {
        if(optional(*object,"octaryn_environment",value))
          require(self.environments_.emplace(index,read_map_scene_environment(value)).second,"duplicate scene environment");
        return;
      }
      Forward forward;
      if(optional(*object,"octaryn_lighting_basis",value)) {
        simdjson::dom::object basis;require(value.get_object().get(basis)==simdjson::SUCCESS,"lighting basis must be an object");
        for(auto item:basis)require(item.key=="version" || item.key=="mode","unknown lighting basis field");
        require(number(field(basis,"version"))==1,"unsupported lighting basis version");
        std::string_view mode;require(field(basis,"mode").get_string().get(mode)==simdjson::SUCCESS && mode=="zero-tangent-plane","unsupported lighting basis mode");
        require(self.zero_basis_.insert(index).second,"duplicate lighting basis declaration");
      }
      if(optional(*object,"octaryn_blend",value)) {
        std::string_view mode;require(value.get_string().get(mode)==simdjson::SUCCESS && mode=="additive","unsupported map blend mode");
        forward.additive=true;
      }
      if(optional(*object,"octaryn_view_fade",value)) {
        simdjson::dom::object fade;require(value.get_object().get(fade)==simdjson::SUCCESS,"view fade must be an object");
        for(auto item:fade)require(item.key=="version" || item.key=="parameters","unknown view fade field");
        require(number(field(fade,"version"))==1,"unsupported view fade version");
        simdjson::dom::array parameters;
        require(field(fade,"parameters").get_array().get(parameters)==simdjson::SUCCESS && parameters.size()==4,"view fade requires four parameters");
        unsigned lane{};
        for(auto component:parameters) {
          double number{};require(component.get_double().get(number)==simdjson::SUCCESS && std::isfinite(number) && number>=(lane<2?-1:0) && number<=1,"view fade parameter out of bounds");
          forward.parameters[lane++]=float(number);
        }
        require(forward.parameters[0]!=forward.parameters[1],"view fade endpoints must differ");forward.view_fade=true;
      }
      if(forward.additive || forward.view_fade)require(self.forward_.emplace(index,forward).second,"duplicate forward material declaration");
      if(optional(*object,"octaryn_material_layers",value))
        require(self.materials_.emplace(index,read(value)).second,"duplicate layer material declaration");
    }catch(const std::exception& error) {self.error_=error.what();}
  }
public:
  void bind(fastgltf::Parser& parser) {parser.setUserPointer(this);parser.setExtrasParseCallback(extras);}
  void validate() const {collision_.validate();if(!error_.empty())throw std::runtime_error(error_);}
  bool collision(std::size_t mesh) const {return collision_.enabled(mesh);}
  bool zero_basis(std::size_t material) const {validate();return zero_basis_.contains(material);}
  const MapLayerSourceMaterial* find(std::size_t index) const {
    validate();const auto found=materials_.find(index);return found==materials_.end()?nullptr:&found->second;
  }
  void forward(std::size_t index,bool& additive,bool& fade,std::array<float,4>& parameters) const {
    validate();const auto found=forward_.find(index);if(found==forward_.end())return;
    additive=found->second.additive;fade=found->second.view_fade;parameters=found->second.parameters;
  }
  bool empty() const {validate();return materials_.empty();}
  MapSceneEnvironment environment(std::size_t scene) const {
    validate();const auto found=environments_.find(scene);
    return found==environments_.end()?MapSceneEnvironment{}:found->second;
  }
};
}
