#include "SceneCatalog.h"
#include "FilePath.h"
#include "SceneResourceHash.h"
#include "../MapWorld/MapSource.h"
#include "../MapWorld/MapMaterials.h"
#include "../MapWorld/MapTextureCache.h"
#include <fastgltf/core.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
std::string utf8(const std::filesystem::path& path) {
  const auto value=path.generic_u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};
}
std::array<float,6> empty_bounds() {
  const auto far=std::numeric_limits<float>::max();return {far,far,far,-far,-far,-far};
}
void grow(std::array<float,6>& bounds,const std::array<float,6>& added) {
  for(unsigned axis=0;axis<3;++axis) {bounds[axis]=std::min(bounds[axis],added[axis]);bounds[axis+3]=std::max(bounds[axis+3],added[axis+3]);}
}
std::array<float,6> accessor_bounds(const fastgltf::Accessor& accessor) {
  require(accessor.min && accessor.max && accessor.min->size()==3 && accessor.max->size()==3,"scene POSITION accessor requires authored bounds");
  const auto number=[](const fastgltf::AccessorBoundsArray& values,unsigned index) {
    return values.isType<double>()?float(values.get<double>(index)):float(values.get<std::int64_t>(index));
  };
  std::array<float,6> result;
  for(unsigned axis=0;axis<3;++axis) {result[axis]=number(*accessor.min,axis);result[axis+3]=number(*accessor.max,axis);}
  return result;
}
std::array<float,6> transformed(const std::array<float,6>& bounds,const std::array<float,16>& matrix) {
  auto output=empty_bounds();
  for(unsigned corner=0;corner<8;++corner) {
    const float point[3]={bounds[(corner&1)?3:0],bounds[(corner&2)?4:1],bounds[(corner&4)?5:2]};
    for(unsigned axis=0;axis<3;++axis) {
      const float value=matrix[axis]*point[0]+matrix[4+axis]*point[1]+matrix[8+axis]*point[2]+matrix[12+axis];
      output[axis]=std::min(output[axis],value);output[axis+3]=std::max(output[axis+3],value);
    }
  }
  return output;
}
}
bool import_scene_catalog(const std::filesystem::path& source,SceneCatalog& output,std::string& error,const std::atomic_bool* cancel) {
  try {
    require(!cancel || !cancel->load(),"scene catalog preparation canceled");
    const auto path=content::canonical_file_path(source);MapSourceInfo info;
    if(!inspect_map_source(path,info,error))return false;
    auto data=fastgltf::MappedGltfFile::FromPath(content::file_io_path(path));require(data.error()==fastgltf::Error::None,"scene source map failed");
    fastgltf::Parser parser(fastgltf::Extensions::EXT_meshopt_compression | fastgltf::Extensions::KHR_texture_transform |
        fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_unlit);
    MapLayerImport layers;layers.bind(parser);auto loaded=parser.loadGltf(data.get(),path.parent_path(),fastgltf::Options::None);
    require(loaded.error()==fastgltf::Error::None,"scene metadata parsing failed");layers.validate();const auto& asset=loaded.get();
    SceneCatalog catalog;catalog.source=utf8(path);catalog.mesh_count=unsigned(info.mesh_count);catalog.material_count=unsigned(info.material_count);
    catalog.instanced_triangles=info.instanced_triangles;
    // Preserve source mesh IDs, but only default-scene instances own cooked geometry.
    std::vector<bool> active_meshes(catalog.mesh_count);
    for(const auto& instance:info.instances)active_meshes.at(instance.mesh)=true;
    std::set<std::filesystem::path> resources{path};
    const auto add=[&](const fastgltf::DataSource& resource) {
      if(const auto* uri=std::get_if<fastgltf::sources::URI>(&resource)) {
        require(uri->uri.isLocalPath(),"scene resources must be local files");
        resources.insert(content::canonical_file_path(path.parent_path()/uri->uri.fspath()));
      }
    };
    for(const auto& buffer:asset.buffers)add(buffer.data);
    for(const auto& image:asset.images)add(image.data);
    std::string identity;
    for(const auto& resource:resources) {
      const auto hash=scene_resource_hash(resource,error,cancel);if(hash.empty())return false;
      catalog.resources.push_back({utf8(resource),hash,std::filesystem::file_size(content::file_io_path(resource))});identity+=hash;
    }
    catalog.source_hash=map_texture_digest({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()});
    std::vector<std::array<float,6>> mesh_bounds(catalog.mesh_count,empty_bounds());
    for(const auto& primitive:info.primitives) {
      if(!active_meshes.at(primitive.mesh))continue;
      catalog.unique_triangles+=primitive.triangles;
      const auto& raw=asset.meshes[primitive.mesh].primitives[primitive.primitive];
      require(raw.targets.empty(),"static scene catalog cannot silently discard morph targets");
      const auto position=raw.findAttribute("POSITION");require(position!=raw.attributes.end(),"scene positions missing");
      ScenePrimitive entry;entry.mesh=unsigned(primitive.mesh);entry.primitive=unsigned(primitive.primitive);
      entry.material=raw.materialIndex?unsigned(*raw.materialIndex):invalid_id;
      entry.position_only=raw.attributes.size()==1;
      entry.vertices=primitive.vertices;entry.triangles=primitive.triangles;entry.surface=load_map_material(asset,raw,&layers);
      entry.bounds=accessor_bounds(asset.accessors[position->accessorIndex]);grow(mesh_bounds[entry.mesh],entry.bounds);
      entry.first_part=unsigned(catalog.parts.size());entry.part_count=unsigned((entry.triangles+catalog.part_triangles-1)/catalog.part_triangles);
      for(std::uint64_t first=0;first<entry.triangles;first+=catalog.part_triangles)
        catalog.parts.push_back({unsigned(catalog.primitives.size()),first,std::min<std::uint64_t>(catalog.part_triangles,entry.triangles-first),entry.bounds});
      catalog.primitives.push_back(entry);
    }
    for(const auto& source_instance:info.instances) {
      require(!asset.nodes[source_instance.node].skinIndex,"static scene catalog cannot silently discard skinning");
      catalog.instances.push_back({unsigned(source_instance.node),unsigned(source_instance.mesh),source_instance.name,
          source_instance.transform,transformed(mesh_bounds[source_instance.mesh],source_instance.transform)});
    }
    if(!validate_scene_catalog(catalog,error))return false;
    std::printf("scene_catalog_complete meshes=%u primitives=%zu parts=%zu instances=%zu unique_triangles=%llu instanced_triangles=%llu source_expansion=0\n",
        catalog.mesh_count,catalog.primitives.size(),catalog.parts.size(),catalog.instances.size(),
        static_cast<unsigned long long>(catalog.unique_triangles),static_cast<unsigned long long>(catalog.instanced_triangles));
    output=std::move(catalog);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
