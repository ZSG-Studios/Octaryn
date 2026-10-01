#include "SceneCollisionCatalog.h"
#include "ResourceDigest.h"
#include "FilePath.h"
#include <glaze/glaze.hpp>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace octaryn::character_motion {
struct CollisionCatalogResource {std::string path,hash;std::uint64_t bytes{};};
struct CollisionCatalogPrimitive {
  std::uint32_t mesh{},primitive{},first_part{},part_count{};std::uint64_t triangles{};
  scene_geometry::Bounds bounds{};std::string triangle_order,triangle_order_hash;
};
struct CollisionCatalogPart {
  std::uint32_t primitive{};std::uint64_t first_triangle{},triangle_count{};
  scene_geometry::Bounds bounds{};bool bounds_prepared{};
};
struct CollisionCatalogJson {
  std::uint32_t version{},mesh_count{};std::uint64_t unique_triangles{},instanced_triangles{};
  std::string source,source_hash;
  std::vector<CollisionCatalogResource> resources;
  std::vector<CollisionCatalogPrimitive> primitives;
  std::vector<CollisionCatalogPart> parts;
  std::vector<scene_geometry::Instance> instances;
};
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
std::filesystem::path path(const std::string& value) {return std::filesystem::path(reinterpret_cast<const char8_t*>(value.c_str()));}
void valid_bounds(const scene_geometry::Bounds& bounds) {
  for(const auto value:bounds)require(std::isfinite(value),"collision catalog bounds are not finite");
  for(unsigned axis=0;axis<3;++axis)require(bounds[axis]<=bounds[axis+3],"collision catalog bounds inverted");
}
}
bool read_scene_collision_catalog(const std::filesystem::path& file,const std::filesystem::path& source,
    SceneCollisionCatalog& output,std::string& error,const std::atomic_bool* cancel) {
  try {
    require(!cancel || !cancel->load(),"scene collision validation canceled");
    const auto size=std::filesystem::file_size(content::file_io_path(file));
    require(size && size<=256ull*1024*1024,"collision catalog metadata exceeds limit");
    std::ifstream input(content::file_io_path(file),std::ios::binary);std::string text(size,'\0');
    require(bool(input.read(text.data(),std::streamsize(size))),"collision catalog could not be read");
    CollisionCatalogJson json;constexpr glz::opts options{.error_on_unknown_keys=false};
    require(!glz::read<options>(json,text),"collision catalog JSON invalid");
    require(json.version==2 && json.mesh_count && json.mesh_count<=1000000 && !json.parts.empty() &&
        json.parts.size()<=1000000 && !json.instances.empty() && json.instances.size()<=1000000 &&
        json.primitives.size()<=1000000 && !json.resources.empty() && json.resources.size()<=4096,"collision catalog limits invalid");
    require(std::filesystem::equivalent(content::file_io_path(path(json.source)),content::file_io_path(source)),"collision catalog belongs to a different source");
    std::set<std::string> resources;std::string identity;
    for(const auto& resource:json.resources) {
      require(resources.insert(resource.path).second && resource.hash.size()==64,"collision resource identity duplicated or invalid");
      require(std::filesystem::file_size(content::file_io_path(path(resource.path)))==resource.bytes,"collision source resource size changed");
      const auto hash=content::resource_tree_digest(path(resource.path),error,1ull<<40,cancel);
      require(!cancel || !cancel->load(),"scene collision validation canceled");
      require(hash==resource.hash,"collision source resource content changed");
      identity+=resource.hash;
    }
    require(resources.contains(json.source) && content::resource_digest({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()})==json.source_hash,
        "collision catalog source identity differs");
    SceneCollisionCatalog result;result.source=source;std::uint64_t unique{};std::size_t next{};
    std::vector<std::uint64_t> mesh_triangles(json.mesh_count);
    for(std::uint32_t index=0;index<json.primitives.size();++index) {
      const auto& primitive=json.primitives[index];
      require(primitive.mesh<json.mesh_count && primitive.first_part==next && primitive.part_count &&
          primitive.part_count<=json.parts.size()-next,"collision primitive part range invalid");
      SceneCollisionOrder order;
      require(primitive.triangle_order.empty()==primitive.triangle_order_hash.empty(),"collision triangle order identity incomplete");
      if(!primitive.triangle_order.empty()) {
        const auto relative=path(primitive.triangle_order);
        require(!relative.is_absolute() && !relative.has_root_name(),"collision triangle order path must be relative");
        for(const auto& component:relative)require(component!="..","collision triangle order path escapes catalog");
        order.path=std::filesystem::canonical(content::file_io_path(file.parent_path()/relative));
        const auto confined=order.path.lexically_relative(std::filesystem::canonical(content::file_io_path(file.parent_path())));
        require(!confined.empty(),"collision triangle order path is outside catalog");
        for(const auto& component:confined)require(component!="..","collision triangle order link escapes catalog");
        order.config.source_hash=json.source_hash;order.config.mesh=primitive.mesh;order.config.primitive=primitive.primitive;
        order.config.triangles=primitive.triangles;order.config.bounds=primitive.bounds;
        order.stamp=std::filesystem::last_write_time(order.path);order.bytes=std::filesystem::file_size(order.path);
        require(content::resource_tree_digest(order.path,error,1ull<<40,cancel)==primitive.triangle_order_hash,
            "collision triangle order content changed");
        if(!scene_geometry::validate_spatial_triangle_order(order.path,order.config,error,cancel))throw std::runtime_error(error);
        require(order.stamp==std::filesystem::last_write_time(order.path) && order.bytes==std::filesystem::file_size(order.path),
            "collision triangle order changed during validation");
      }
      result.orders.push_back(std::move(order));
      std::uint64_t first{};
      for(unsigned item=0;item<primitive.part_count;++item) {
        const auto& part=json.parts[next++];
        require(part.primitive==index && part.first_triangle==first && part.triangle_count && part.triangle_count<=65536,
            "collision catalog triangle coverage invalid");
        valid_bounds(part.bounds);scene_geometry::Part converted;
        converted.mesh=primitive.mesh;converted.primitive=primitive.primitive;converted.first_triangle=first;
        converted.triangle_count=part.triangle_count;converted.bounds=part.bounds;converted.bounds_prepared=part.bounds_prepared;
        converted.cooked=true;converted.reservation_bytes=1;result.parts.push_back(converted);result.part_orders.push_back(index);
        first+=part.triangle_count;
      }
      require(first==primitive.triangles,"collision catalog primitive coverage incomplete");
      unique+=first;mesh_triangles[primitive.mesh]+=first;
    }
    require(next==json.parts.size() && unique==json.unique_triangles,"collision catalog source coverage incomplete");
    std::set<std::uint32_t> nodes;std::uint64_t expanded{};
    for(const auto& instance:json.instances) {
      require(instance.mesh<json.mesh_count && nodes.insert(instance.node).second,"collision instance identity invalid");
      valid_bounds(instance.bounds);expanded+=mesh_triangles[instance.mesh];
    }
    require(expanded==json.instanced_triangles,"collision catalog instance coverage incomplete");
    result.instances=std::move(json.instances);output=std::move(result);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
