#pragma once
#include <fastgltf/core.hpp>
#include <simdjson.h>
#include <map>
#include <stdexcept>
#include <string>
namespace octaryn::assets {
class GltfCollisionImport {
  std::map<std::size_t,bool> meshes_;
  std::string error_;
  static void require(bool okay,const char* error) {if(!okay)throw std::runtime_error(error);}
  static void callback(simdjson::dom::object* object,std::size_t index,fastgltf::Category category,void* pointer) {
    static_cast<GltfCollisionImport*>(pointer)->extras(object,index,category);
  }
public:
  void extras(simdjson::dom::object* object,std::size_t index,fastgltf::Category category) {
    if(category!=fastgltf::Category::Meshes || !error_.empty())return;
    try {
      simdjson::dom::element value;const auto found=(*object)["octaryn_collision"].get(value);
      if(found==simdjson::NO_SUCH_FIELD)return;
      require(found==simdjson::SUCCESS,"invalid mesh collision declaration");
      simdjson::dom::object declaration;
      require(value.get_object().get(declaration)==simdjson::SUCCESS,"mesh collision declaration must be an object");
      for(auto field:declaration)require(field.key=="version" || field.key=="enabled","unknown mesh collision field");
      std::uint64_t version{};bool enabled{};
      require(declaration["version"].get_uint64().get(version)==simdjson::SUCCESS && version==1,"unsupported mesh collision version");
      require(declaration["enabled"].get_bool().get(enabled)==simdjson::SUCCESS,"mesh collision enabled must be a boolean");
      require(meshes_.emplace(index,enabled).second,"duplicate mesh collision declaration");
    }catch(const std::exception& failure){error_=failure.what();}
  }
  void bind(fastgltf::Parser& parser){parser.setUserPointer(this);parser.setExtrasParseCallback(callback);}
  void validate() const {if(!error_.empty())throw std::runtime_error(error_);}
  bool enabled(std::size_t mesh) const {
    validate();const auto found=meshes_.find(mesh);return found==meshes_.end() || found->second;
  }
};
}
