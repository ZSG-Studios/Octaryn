#include "MapSource.h"
#include "FilePath.h"
#include "GltfBufferViews.h"
#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace octaryn::client::rendering {
namespace {
using namespace fastgltf;
using namespace fastgltf::math;
void require(bool valid,const char* reason) {if(!valid)throw std::runtime_error(reason);}
void add(std::uint64_t& total,std::uint64_t count) {
  require(count<=std::numeric_limits<std::uint64_t>::max()-total,"map source count overflow");total+=count;
}
MapSourcePrimitive describe(const Asset& asset,const Primitive& primitive) {
  const auto position=primitive.findAttribute("POSITION");
  require(position!=primitive.attributes.end() && position->accessorIndex<asset.accessors.size(),"map primitive has no positions");
  const auto vertices=asset.accessors[position->accessorIndex].count;
  require(!primitive.indicesAccessor || *primitive.indicesAccessor<asset.accessors.size(),"map index accessor out of range");
  const auto indices=primitive.indicesAccessor?asset.accessors[*primitive.indicesAccessor].count:vertices;
  require(indices>=3,"map primitive has fewer than three indices");
  std::size_t triangles{};
  if(primitive.type==PrimitiveType::Triangles) {require(indices%3==0,"map primitive has partial triangles");triangles=indices/3;}
  else if(primitive.type==PrimitiveType::TriangleStrip || primitive.type==PrimitiveType::TriangleFan)triangles=indices-2;
  else throw std::runtime_error("map requires triangle, strip or fan primitives");
  require(triangles<=std::numeric_limits<std::size_t>::max()/3,"map triangle count overflow");
  const auto decoded=primitive.findAttribute("NORMAL")==primitive.attributes.end()?triangles*3:vertices;
  return {0,0,vertices,triangles,decoded};
}
void geometry_budget(std::uint64_t vertices,std::uint64_t triangles,const MapLoadLimits& limits) {
  require(vertices<=limits.geometry_bytes/sizeof(MapVertex),
      "This scene requires a streamed instance cook: vertex data exceeds the bounded preparation budget.");
  require(triangles<=(limits.geometry_bytes-vertices*sizeof(MapVertex))/(3*sizeof(std::uint32_t)),
      "This scene requires a streamed instance cook: geometry exceeds the bounded preparation budget.");
}
fmat4x4 transform(const Node& node) {
  if(const auto* trs=std::get_if<TRS>(&node.transform))
    return scale(rotate(translate(fmat4x4(),trs->translation),trs->rotation),trs->scale);
  return std::get<fmat4x4>(node.transform);
}
template<class Visitor> void walk(const Asset& asset,std::size_t index,const fmat4x4& parent,
    std::vector<bool>& active,std::size_t depth,const Visitor& visit) {
  require(depth<=1024 && index<asset.nodes.size() && !active[index],"invalid map node hierarchy");
  active[index]=true;const auto& node=asset.nodes[index];const auto world=parent*transform(node);
  if(node.meshIndex) {require(*node.meshIndex<asset.meshes.size(),"map mesh index out of range");visit(index,node,world);}
  for(const auto child:node.children)walk(asset,child,world,active,depth+1,visit);
  active[index]=false;
}
template<class Visitor> void scene(const Asset& asset,const Visitor& visit) {
  const auto selected=asset.defaultScene.value_or(0);
  require(selected<asset.scenes.size(),"map default scene out of range");
  std::vector<bool> active(asset.nodes.size());
  for(const auto node:asset.scenes[selected].nodeIndices)walk(asset,node,fmat4x4(),active,0,visit);
}
}
void qualify_map_primitive(const fastgltf::Asset& asset,const fastgltf::Primitive& primitive,const MapLoadLimits& limits) {
  const auto info=describe(asset,primitive);
  geometry_budget(info.decoded_vertices,info.triangles,limits);
  require(info.triangles<=limits.triangles && info.decoded_vertices<=limits.triangles*3,
      "This scene requires a streamed instance cook: a primitive exceeds the bounded map preparation limit.");
  for(const auto& attribute:primitive.attributes) {
    require(attribute.accessorIndex<asset.accessors.size(),"map attribute accessor out of range");
    require(asset.accessors[attribute.accessorIndex].count<=limits.accessor_elements,"map accessor exceeds preparation limit");
  }
  if(primitive.indicesAccessor)require(asset.accessors[*primitive.indicesAccessor].count<=limits.accessor_elements,
      "map index accessor exceeds preparation limit");
}
void qualify_map_scene(const fastgltf::Asset& asset,const MapLoadLimits& limits,bool catalog) {
  std::uint64_t triangles{},vertices{},primitives{};
  scene(asset,[&](std::size_t,const fastgltf::Node& node,const fastgltf::math::fmat4x4&) {
    if(limits.cancel && limits.cancel->load(std::memory_order_relaxed))throw std::runtime_error("map preparation cancelled");
    for(const auto& primitive:asset.meshes[*node.meshIndex].primitives) {
      add(primitives,1);
      require(primitives<=limits.primitives,"This scene requires a streamed instance cook: too many map primitives.");
      if(catalog)continue;
      qualify_map_primitive(asset,primitive,limits);const auto info=describe(asset,primitive);
      add(triangles,info.triangles);add(vertices,info.decoded_vertices);
      geometry_budget(vertices,triangles,limits);
      require(triangles<=limits.triangles && vertices<=limits.triangles*3,
          "This scene requires a streamed instance cook: flattening its instances exceeds the bounded map preparation limit.");
    }
  });
}
bool inspect_map_source(const std::filesystem::path& path,MapSourceInfo& output,std::string& error) {
  try {
    require(std::filesystem::file_size(content::file_io_path(path))<=512ull*1024*1024,"map metadata exceeds preparation limit");
    auto data=fastgltf::MappedGltfFile::FromPath(content::file_io_path(path));
    require(data.error()==fastgltf::Error::None,"cannot map map source");
    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform | fastgltf::Extensions::KHR_materials_emissive_strength |
        fastgltf::Extensions::EXT_meshopt_compression);
    auto loaded=parser.loadGltf(data.get(),path.parent_path(),fastgltf::Options::None);
    if(loaded.error()!=fastgltf::Error::None)throw std::runtime_error(std::string(fastgltf::getErrorMessage(loaded.error())));
    const auto& asset=loaded.get();octaryn::assets::validate_gltf_accessors(asset);
    require(fastgltf::validate(asset)==fastgltf::Error::None,"invalid map source");
    require(asset.nodes.size()<=1000000 && asset.meshes.size()<=1000000,"map source metadata exceeds inspection budget");
    MapSourceInfo result;result.mesh_count=asset.meshes.size();result.material_count=asset.materials.size();
    for(const auto& buffer:asset.buffers) {
      add(result.logical_bytes,buffer.byteLength);
      if(const auto* uri=std::get_if<fastgltf::sources::URI>(&buffer.data)) {
        require(uri->uri.isLocalPath(),"map source requires local external buffers");
        const auto size=std::filesystem::file_size(content::file_io_path(path.parent_path()/uri->uri.fspath()));
        require(uri->fileByteOffset<=size && buffer.byteLength<=size-uri->fileByteOffset,"external map buffer is truncated");
        add(result.external_bytes,buffer.byteLength);
      }
    }
    for(const auto& view:asset.bufferViews)if(view.meshoptCompression)++result.compressed_views;
    std::vector<std::uint64_t> mesh_triangles(asset.meshes.size());
    for(std::size_t mesh=0;mesh<asset.meshes.size();++mesh)for(std::size_t primitive=0;primitive<asset.meshes[mesh].primitives.size();++primitive) {
      require(result.primitives.size()<1000000,"map primitive metadata exceeds inspection budget");
      auto info=describe(asset,asset.meshes[mesh].primitives[primitive]);info.mesh=mesh;info.primitive=primitive;
      add(result.unique_triangles,info.triangles);add(mesh_triangles[mesh],info.triangles);result.primitives.push_back(info);
    }
    scene(asset,[&](std::size_t index,const fastgltf::Node& node,const fastgltf::math::fmat4x4& matrix) {
      require(result.instances.size()<1000000,"map instance metadata exceeds inspection budget");
      MapSourceInstance instance;instance.node=index;instance.mesh=*node.meshIndex;instance.name=node.name;
      for(std::size_t column=0;column<4;++column)for(std::size_t row=0;row<4;++row) {
        require(std::isfinite(matrix[column][row]),"nonfinite map instance transform");instance.transform[column*4+row]=matrix[column][row];
      }
      add(result.instanced_triangles,mesh_triangles[instance.mesh]);result.instances.push_back(std::move(instance));
    });
    output=std::move(result);error.clear();return true;
  }catch(const std::exception& exception) {error=exception.what();return false;}
}
}
