#include "MapModel.h"
#include "FilePath.h"
#include "MapMaterials.h"
#include "GltfBufferViews.h"
#include "MapSource.h"
#include "MapSourceRange.h"
#include "MapZeroBasis.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>

namespace octaryn::client::rendering {
namespace {
using namespace fastgltf;
using namespace fastgltf::math;
constexpr std::uint64_t max_encoded_image_bytes=86ull*1024*1024;
constexpr std::size_t max_map_images=1024;
void check(bool value,const char* reason) { if(!value) throw std::runtime_error(reason); }
template<class T> std::vector<T> values(const Asset& asset,size_t index,AccessorType type,const octaryn::assets::GltfBufferViews& buffers) {
  check(index<asset.accessors.size(),"accessor index out of range");
  const auto& accessor=asset.accessors[index];
  check(accessor.type==type && accessor.count<=24000000,"unsupported accessor shape or size");
  std::vector<T> result;result.reserve(accessor.count);
  iterateAccessor<T>(asset,accessor,[&](T value) {result.push_back(value);},buffers);
  return result;
}
size_t attribute(const Primitive& primitive,const char* name) {
  const auto found=primitive.findAttribute(name);
  check(found!=primitive.attributes.end(),"missing map vertex attribute");
  return found->accessorIndex;
}
template<class T> std::vector<T> optional_values(const Asset& asset,const Primitive& primitive,
    const char* name,AccessorType type,size_t count,const octaryn::assets::GltfBufferViews& buffers) {
  const auto found=primitive.findAttribute(name);
  if(found==primitive.attributes.end())return {};
  auto result=values<T>(asset,found->accessorIndex,type,buffers);
  check(result.size()==count,"map vertex attribute count mismatch");
  return result;
}
fvec3 unit_normal(const fvec3& value) {
  const auto magnitude=std::max({std::abs(value[0]),std::abs(value[1]),std::abs(value[2])});
  check(std::isfinite(magnitude),"nonfinite map normal");
  if(magnitude==0.f)return fvec3(0.f,1.f,0.f);
  const auto scaled=value/magnitude;
  return scaled/std::sqrt(dot(scaled,scaled));
}
std::vector<std::uint32_t> triangle_indices(const Asset& asset,const Primitive& primitive,
    size_t vertex_count,size_t remaining_indices,bool mirrored,const octaryn::assets::GltfBufferViews& buffers) {
  check(primitive.type==PrimitiveType::Triangles || primitive.type==PrimitiveType::TriangleStrip ||
      primitive.type==PrimitiveType::TriangleFan,"map requires triangle, strip or fan primitives");
  std::vector<std::uint32_t> source;
  if(primitive.indicesAccessor)source=values<std::uint32_t>(asset,*primitive.indicesAccessor,AccessorType::Scalar,buffers);
  else {
    source.resize(vertex_count);
    for(size_t i=0;i<vertex_count;++i)source[i]=static_cast<std::uint32_t>(i);
  }
  for(const auto index:source)check(index<vertex_count,"map vertex index out of range");
  check(source.size()>=3,"map primitive has fewer than three indices");
  if(primitive.type==PrimitiveType::Triangles) {
    check(source.size()%3==0,"map indices are not triangles");
    check(source.size()<=remaining_indices,"too many map triangles");
  } else {
    check(source.size()-2<=remaining_indices/3,"too many map triangles");
    std::vector<std::uint32_t> triangles;
    triangles.reserve((source.size()-2)*3);
    for(size_t i=0;i+2<source.size();++i) {
      if(primitive.type==PrimitiveType::TriangleFan)
        triangles.insert(triangles.end(),{source[0],source[i+1],source[i+2]});
      else triangles.insert(triangles.end(),{source[i],source[i+1+i%2],source[i+2-i%2]});
    }
    source=std::move(triangles);
  }
  if(mirrored)for(size_t i=0;i<source.size();i+=3)std::swap(source[i+1],source[i+2]);
  return source;
}
fmat4x4 node_matrix(const Node& node) {
  if(std::holds_alternative<TRS>(node.transform)) {
    const auto& trs=std::get<TRS>(node.transform);
    return scale(rotate(translate(fmat4x4(),trs.translation),trs.rotation),trs.scale);
  }
  return std::get<fmat4x4>(node.transform);
}
std::string mime_string(MimeType mime,const sources::URI* uri) {
  if(mime!=MimeType::None)return std::string(getMimeTypeString(mime));
  const auto extension=uri?uri->uri.fspath().extension():std::filesystem::path();
  if(extension==".png")return "image/png";
  if(extension==".jpg" || extension==".jpeg")return "image/jpeg";
  return {};
}
void check_supported_mime(const std::string& mime) {
  check(mime!="image/ktx2" && mime!="image/vnd-ms.dds" && mime!="image/webp",
      "compressed map image formats are unsupported");
}
void reserve_image_bytes(std::uint64_t count,std::uint64_t& retained,std::uint64_t budget) {
  check(count>0 && count<=max_encoded_image_bytes && count<=std::numeric_limits<int>::max(),
      "map encoded image exceeds size bound or is empty");
  check(count<=budget-retained,"map encoded images exceed aggregate size bound");
  retained+=count;
}
void load_image_bytes(std::span<const std::byte> source,MimeType mime,MapModelImage& target,
    std::uint64_t& retained,std::uint64_t budget) {
  reserve_image_bytes(source.size(),retained,budget);
  target.bytes.resize(source.size());
  std::memcpy(target.bytes.data(),source.data(),source.size());
  if(mime!=MimeType::None)target.mime_type=std::string(getMimeTypeString(mime));
}
void load_images(const Asset& asset,const std::filesystem::path& parent,MapModel& model,const MapLoadLimits& limits,octaryn::assets::GltfBufferViews& buffers,const std::vector<bool>* used=nullptr) {
  check(asset.images.size()<=max_map_images,"too many map images");
  model.images.resize(asset.images.size());
  std::uint64_t retained=0;
  for(size_t index=0;index<asset.images.size();++index) {
    if(used && !(*used)[index])continue;
    check(!limits.cancel || !limits.cancel->load(std::memory_order_relaxed),"map preparation cancelled");
    buffers.clear();auto& target=model.images[index];
    const auto& data=asset.images[index].data;
    if(const auto* view=std::get_if<sources::BufferView>(&data)) {
      check(view->bufferViewIndex<asset.bufferViews.size(),"map image buffer view out of range");
      const auto bytes=buffers(asset,view->bufferViewIndex);
      check_supported_mime(mime_string(view->mimeType,nullptr));
      load_image_bytes({bytes.data(),bytes.size()},view->mimeType,target,retained,limits.encoded_bytes);
    } else if(const auto* uri=std::get_if<sources::URI>(&data)) {
      check_supported_mime(mime_string(uri->mimeType,uri));
      target.mime_type=mime_string(uri->mimeType,uri);
      std::ifstream file(parent/uri->uri.fspath(),std::ios::binary|std::ios::ate);
      check(file.good(),"cannot read external map image");
      const auto end=file.tellg();
      check(end>=std::streampos(0),"cannot size external map image");
      const auto size=static_cast<std::uint64_t>(static_cast<std::streamoff>(end));
      check(uri->fileByteOffset<=size,"external map image offset exceeds file");
      const auto count=size-uri->fileByteOffset;
      reserve_image_bytes(count,retained,limits.encoded_bytes);
      target.bytes.resize(static_cast<std::size_t>(count));
      file.seekg(static_cast<std::streamoff>(uri->fileByteOffset));
      check(file.good(),"cannot seek external map image");
      file.read(reinterpret_cast<char*>(target.bytes.data()),static_cast<std::streamsize>(count));
      check(file.good() && file.gcount()==static_cast<std::streamsize>(count),"short external map image read");
    } else if(const auto* array=std::get_if<sources::Array>(&data)) {
      check_supported_mime(mime_string(array->mimeType,nullptr));
      load_image_bytes(std::span<const std::byte>(array->bytes.data(),array->bytes.size()),array->mimeType,target,retained,limits.encoded_bytes);
    } else if(const auto* byte_view=std::get_if<sources::ByteView>(&data)) {
      check_supported_mime(mime_string(byte_view->mimeType,nullptr));
      load_image_bytes(byte_view->bytes,byte_view->mimeType,target,retained,limits.encoded_bytes);
    } else check(false,"unsupported map image source");
  }
}
void add_mesh(const Asset& asset,const Mesh& mesh,const fmat4x4& world,MapModel& model,const MapLoadLimits& limits,octaryn::assets::GltfBufferViews& buffers,const MapLayerImport& layers,size_t selected=SIZE_MAX,size_t source_node=SIZE_MAX) {
  if(source_node!=SIZE_MAX && std::binary_search(limits.excluded_nodes.begin(),limits.excluded_nodes.end(),std::string(asset.nodes[source_node].name)))return;
  const auto determinant_value=determinant(mat<float,3,3>(world));
  check(std::isfinite(determinant_value) && determinant_value!=0.f,"singular or nonfinite map transform");
  const auto normal_matrix=transpose(inverse(mat<float,3,3>(world)));
  for(size_t primitive_index=0;primitive_index<mesh.primitives.size();++primitive_index) {
    if(selected!=SIZE_MAX && selected!=primitive_index)continue;
    const auto& primitive=mesh.primitives[primitive_index];buffers.clear();
    check(!limits.cancel || !limits.cancel->load(std::memory_order_relaxed),"map preparation cancelled");
    check(primitive.targets.empty(),"map nodes must be static (morph targets are unsupported)");
    check(model.primitives.size()<limits.primitives,"too many map primitives");
    const auto positions=values<fvec3>(asset,attribute(primitive,"POSITION"),AccessorType::Vec3,buffers);
    check(!positions.empty(),"empty map primitive");
    const auto normals=optional_values<fvec3>(asset,primitive,"NORMAL",AccessorType::Vec3,positions.size(),buffers);
    const auto uvs=optional_values<fvec2>(asset,primitive,"TEXCOORD_0",AccessorType::Vec2,positions.size(),buffers);
    const auto uv1=optional_values<fvec2>(asset,primitive,"TEXCOORD_1",AccessorType::Vec2,positions.size(),buffers);
    for(const char* name:{"_OCTARYN_BLEND0","_OCTARYN_BLEND1"})if(const auto found=primitive.findAttribute(name);found!=primitive.attributes.end()) {
      check(found->accessorIndex<asset.accessors.size(),"layer accessor out of range");const auto& a=asset.accessors[found->accessorIndex];
      check(a.type==AccessorType::Vec4 && a.componentType==ComponentType::Float && !a.normalized,"layer weights require float32 vec4");
    }
    const auto blend0=optional_values<fvec4>(asset,primitive,"_OCTARYN_BLEND0",AccessorType::Vec4,positions.size(),buffers);
    const auto blend1=optional_values<fvec4>(asset,primitive,"_OCTARYN_BLEND1",AccessorType::Vec4,positions.size(),buffers);
    const auto tangents=optional_values<fvec4>(asset,primitive,"TANGENT",AccessorType::Vec4,positions.size(),buffers);
    std::vector<fvec4> colors;
    if(const auto color=primitive.findAttribute("COLOR_0");color!=primitive.attributes.end()) {
      check(color->accessorIndex<asset.accessors.size(),"map color accessor out of range");
      if(asset.accessors[color->accessorIndex].type==AccessorType::Vec3) {
        const auto rgb=values<fvec3>(asset,color->accessorIndex,AccessorType::Vec3,buffers);
        for(const auto& value:rgb)colors.emplace_back(value[0],value[1],value[2],1.f);
      } else colors=values<fvec4>(asset,color->accessorIndex,AccessorType::Vec4,buffers);
      check(colors.size()==positions.size(),"map color count mismatch");
    }
    auto indices=triangle_indices(asset,primitive,positions.size(),limits.triangles*3-model.indices.size(),
        determinant_value<0.f,buffers);
    const auto vertex_count=normals.empty()?indices.size():positions.size();
    check(vertex_count<=limits.triangles*3-model.vertices.size(),"too many map vertices");
    MapPrimitive draw;
    draw.collision=layers.collision(std::size_t(&mesh-asset.meshes.data()));
    draw.material=load_map_material(asset,primitive,&layers);
    const auto raw_t=optional_values<fvec3>(asset,primitive,"_OCTARYN_SOURCE_TANGENT",AccessorType::Vec3,positions.size(),buffers);
    const auto raw_b=optional_values<fvec3>(asset,primitive,"_OCTARYN_SOURCE_BITANGENT",AccessorType::Vec3,positions.size(),buffers);
    if(draw.material.zero_basis) {
      for(const char* name:{"_OCTARYN_SOURCE_TANGENT","_OCTARYN_SOURCE_BITANGENT"}) {
        const auto& a=asset.accessors[attribute(primitive,name)];
        check(a.componentType==ComponentType::Float && !a.normalized,"source basis requires float32 vec3");
      }
      check(tangents.empty(),"declared zero basis forbids regenerated glTF tangent");
      validate_map_zero_basis(positions.size(),normals,uvs,raw_t,raw_b,indices);
    } else check(raw_t.empty() && raw_b.empty(),"authored source basis requires explicit material mode");
    check(draw.material.layer_count?(!blend0.empty() && !blend1.empty()):(blend0.empty() && blend1.empty()),
        "weighted layer attributes and material must be declared together");
    for(const auto& texture:draw.material.textures)if(texture.image>=0)
      check(texture.texcoord==0?!uvs.empty():!uv1.empty(),"map texture references a missing UV attribute");
    draw.source.mesh=std::uint32_t(&mesh-asset.meshes.data());draw.source.primitive=std::uint32_t(primitive_index);
    draw.source.mesh_name=std::string(mesh.name.substr(0,1024));
    if(source_node!=SIZE_MAX) {draw.source.node=std::uint32_t(source_node);draw.source.node_name=std::string(asset.nodes[source_node].name.substr(0,1024));}
    for(unsigned column=0;column<4;++column)for(unsigned row=0;row<4;++row)draw.source.evaluated[column*4+row]=world[column][row];
    for(unsigned axis=0;axis<3;++axis) {draw.source.local_bounds[axis]=1e30f;draw.source.local_bounds[axis+3]=-1e30f;}
    for(const auto& position:positions)for(unsigned axis=0;axis<3;++axis) {
      draw.source.local_bounds[axis]=std::min(draw.source.local_bounds[axis],position[axis]);
      draw.source.local_bounds[axis+3]=std::max(draw.source.local_bounds[axis+3],position[axis]);
    }
    draw.first_index=static_cast<std::uint32_t>(model.indices.size());
    draw.index_count=static_cast<std::uint32_t>(indices.size());
    for(size_t k=0;k<3;++k) {draw.bounds_min[k]=1e30f;draw.bounds_max[k]=-1e30f;}
    const auto vertex_base=static_cast<std::uint32_t>(model.vertices.size());
    std::vector<MapVertex> vertices(positions.size());
    for(size_t i=0;i<positions.size();++i) {
      const auto position=world*fvec4(positions[i][0],positions[i][1],positions[i][2],1.f);
      const auto normal=normals.empty()?fvec3(0.f,1.f,0.f):unit_normal(normal_matrix*normals[i]);
      MapVertex vertex{};
      for(size_t k=0;k<3;++k) {
        check(std::isfinite(position[k]) && std::isfinite(normal[k]),"nonfinite map vertex");
        vertex.position[k]=position[k];vertex.normal[k]=normal[k];
        draw.bounds_min[k]=std::min(draw.bounds_min[k],position[k]);
        draw.bounds_max[k]=std::max(draw.bounds_max[k],position[k]);
      }
      if(!uvs.empty())for(size_t k=0;k<2;++k) {
        check(std::isfinite(uvs[i][k]),"nonfinite map UV");vertex.uv[k]=uvs[i][k];
      }
      if(!uv1.empty())for(size_t k=0;k<2;++k) {
        check(std::isfinite(uv1[i][k]),"nonfinite map UV1");vertex.uv1[k]=uv1[i][k];
      }
      if(!tangents.empty() && !normals.empty()) {
        const auto transformed=mat<float,3,3>(world)*fvec3(tangents[i][0],tangents[i][1],tangents[i][2]);
        const auto tangent=unit_normal(transformed-normal*dot(normal,transformed));
        for(size_t k=0;k<3;++k)vertex.tangent[k]=tangent[k];
        check(tangents[i][3]==1.f || tangents[i][3]==-1.f,"invalid map tangent handedness");
        vertex.tangent[3]=tangents[i][3]*(determinant_value<0.f?-1.f:1.f);
      }
      if(draw.material.zero_basis)for(unsigned axis=0;axis<3;++axis) {
        vertex.tangent[axis]=raw_t[i][axis];vertex.blend0[axis]=raw_b[i][axis];
      }
      for(size_t k=0;k<4;++k) {
        const auto value=colors.empty()?1.f:colors[i][k];
        check(std::isfinite(value),"nonfinite map vertex color");vertex.color[k]=value;
      }
      for(unsigned lane=0;lane<8;++lane)if(draw.material.layer_count) {
        const auto weight=lane<4?blend0[i][lane]:blend1[i][lane-4];
        check(std::isfinite(weight) && weight>=0 && (lane<draw.material.layer_count || weight==0),"invalid authored layer weight");
        (lane<4?vertex.blend0:vertex.blend1)[lane%4]=weight;
      }
      vertices[i]=vertex;
    }
    if(normals.empty()) {
      // glTF requires flat normals when NORMAL is absent, including shared indexed vertices.
      for(size_t i=0;i<indices.size();i+=3) {
        const auto point=[&](size_t corner) {
          const auto& p=vertices[indices[i+corner]].position;
          return fvec3(p[0],p[1],p[2]);
        };
        const auto normal=unit_normal(cross(point(1)-point(0),point(2)-point(0)));
        for(size_t corner=0;corner<3;++corner) {
          auto vertex=vertices[indices[i+corner]];
          for(size_t k=0;k<3;++k)vertex.normal[k]=normal[k];
          model.indices.push_back(static_cast<std::uint32_t>(model.vertices.size()));
          model.vertices.push_back(vertex);
        }
      }
    } else {
      model.vertices.insert(model.vertices.end(),vertices.begin(),vertices.end());
      for(const auto index:indices)model.indices.push_back(index+vertex_base);
    }
    if(draw.collision)model.collision_indices.insert(model.collision_indices.end(),model.indices.begin()+draw.first_index,model.indices.end());
    model.primitives.push_back(std::move(draw));
  }
}
void add_scene_light(const Asset& asset,const Node& node,const fmat4x4& world,MapModel& model) {
  check(*node.lightIndex<asset.lights.size(),"scene light index out of range");
  const auto& source=asset.lights[*node.lightIndex];
  check(source.type==LightType::Point,"only bounded point scene lights are supported");
  check(model.lights.size()<1024 && source.range && std::isfinite(*source.range) &&
      *source.range>0 && *source.range<=100000,"invalid or unbounded scene light range");
  check(std::isfinite(source.intensity) && source.intensity>=0 && source.intensity<=1e8,"invalid scene light intensity");
  WorldLocalLight light;const auto position=world*fvec4(0,0,0,1);
  for(size_t lane=0;lane<3;++lane) {
    check(std::isfinite(position[lane]) && std::abs(position[lane])<=100000,"invalid scene light position");
    check(std::isfinite(source.color[lane]) && source.color[lane]>=0 && source.color[lane]<=100000,"invalid scene light color");
    light.position_range[lane]=position[lane];light.color_intensity[lane]=source.color[lane];
  }
  light.position_range[3]=*source.range;light.color_intensity[3]=source.intensity;
  model.lights.push_back(light);
}
void walk_lights(const Asset& asset,size_t index,const fmat4x4& parent,MapModel& model,size_t depth) {
  check(depth<=1024 && index<asset.nodes.size(),"invalid scene light hierarchy");
  const auto& node=asset.nodes[index];const auto world=parent*node_matrix(node);
  if(node.lightIndex)add_scene_light(asset,node,world,model);
  for(const auto child:node.children)walk_lights(asset,child,world,model,depth+1);
}
void walk_nodes(const Asset& asset,size_t index,const fmat4x4& parent,MapModel& model,size_t depth,bool catalog,const MapLoadLimits& limits,octaryn::assets::GltfBufferViews& buffers,const MapLayerImport& layers) {
  check(depth<=1024 && index<asset.nodes.size(),"invalid map node hierarchy");
  const auto& node=asset.nodes[index];
  check(!node.skinIndex,"map nodes must be static (skinned nodes are unsupported)");
  const fmat4x4 world=parent*node_matrix(node);
  if(node.lightIndex && !catalog)add_scene_light(asset,node,world,model);
  if(node.meshIndex) {
    check(*node.meshIndex<asset.meshes.size(),"map mesh index out of range");
    if(catalog) {
      for(const auto& primitive:asset.meshes[*node.meshIndex].primitives) {
        check(primitive.targets.empty(),"map nodes must be static (morph targets are unsupported)");
        check(model.primitives.size()<limits.primitives,"too many map primitives");
        MapPrimitive entry;entry.material=load_map_material(asset,primitive,&layers);model.primitives.push_back(entry);
      }
    } else add_mesh(asset,asset.meshes[*node.meshIndex],world,model,limits,buffers,layers,SIZE_MAX,index);
  }
  for(const auto child:node.children)walk_nodes(asset,child,world,model,depth+1,catalog,limits,buffers,layers);
}
bool load_map_asset(const std::filesystem::path& path,MapModel& output,std::string& error,bool catalog,const MapLoadLimits& limits,size_t mesh=SIZE_MAX,size_t primitive=SIZE_MAX) {
  try {
    std::optional<MappedGltfFile> mapped;
    std::optional<GltfDataBuffer> ranged;
    if(limits.source_length) {
      const auto bytes=read_map_source_range(path,limits);
      auto data=GltfDataBuffer::FromBytes(bytes.data(),bytes.size());check(data.error()==Error::None,"cannot buffer packed map asset");
      ranged.emplace(std::move(data.get()));
    } else {
      check(!limits.source_offset && std::filesystem::file_size(content::file_io_path(path))<=limits.source_bytes,"map asset exceeds size bound");
      auto data=MappedGltfFile::FromPath(content::file_io_path(path));check(data.error()==Error::None,"cannot map map asset");
      mapped.emplace(std::move(data.get()));
    }
    Parser parser(Extensions::KHR_texture_transform | Extensions::KHR_materials_emissive_strength | Extensions::KHR_materials_unlit | Extensions::EXT_meshopt_compression | Extensions::KHR_lights_punctual);
    MapLayerImport layers;layers.bind(parser);
    auto& data=ranged?static_cast<GltfDataGetter&>(*ranged):static_cast<GltfDataGetter&>(*mapped);
    auto loaded=parser.loadGltf(data,path.parent_path(),Options::None);
    if(loaded.error()!=Error::None)throw std::runtime_error("cannot parse map glTF: "+std::string(getErrorMessage(loaded.error())));
    layers.validate();const auto& asset=loaded.get();octaryn::assets::validate_gltf_accessors(asset);
    check(validate(asset)==Error::None,"invalid map glTF");
    check(!asset.scenes.empty(),"map has no scene");
    const auto scene=asset.defaultScene.value_or(0);
    check(scene<asset.scenes.size(),"map default scene out of range");
    octaryn::assets::GltfBufferViews buffers(path.parent_path(),limits.source_bytes,limits.cancel);
    MapModel result;
    if(mesh==SIZE_MAX) {
      result.environment=layers.environment(scene);
      qualify_map_scene(asset,limits,catalog);
      load_images(asset,path.parent_path(),result,limits,buffers);
      for(const auto node:asset.scenes[scene].nodeIndices)walk_nodes(asset,node,fmat4x4(),result,0,catalog,limits,buffers,layers);
    } else {
      check(mesh<asset.meshes.size() && primitive<asset.meshes[mesh].primitives.size(),"map source primitive out of range");
      qualify_map_primitive(asset,asset.meshes[mesh].primitives[primitive],limits);
      load_images(asset,path.parent_path(),result,limits,buffers);
      add_mesh(asset,asset.meshes[mesh],fmat4x4(),result,limits,buffers,layers,primitive);
    }
    check(!result.primitives.empty(),"map scene has no primitives");
    output=std::move(result);error.clear();return true;
  } catch(const std::exception& exception) {error=exception.what();return false;}
}
}
bool load_map_model(const std::filesystem::path& path,MapModel& output,std::string& error,const MapLoadLimits& limits) {
  return load_map_asset(path,output,error,false,limits);
}
bool load_map_source_primitive(const std::filesystem::path& path,std::size_t mesh,std::size_t primitive,
    MapModel& output,std::string& error,const MapLoadLimits& limits) {
  return load_map_asset(path,output,error,false,limits,mesh,primitive);
}
bool load_map_texture_catalog(const std::filesystem::path& path,MapModel& output,std::string& error) {
  return load_map_asset(path,output,error,true,{});
}
bool load_map_material_resources(const std::filesystem::path& path,std::span<const MapMaterial> materials,
    MapModel& output,std::string& error,const MapLoadLimits& limits) {
  try {
    check(std::filesystem::file_size(content::file_io_path(path))<=limits.source_bytes,"map metadata exceeds material preparation limit");
    check(materials.size()<=limits.primitives,"material count exceeds preparation limit");
    auto data=MappedGltfFile::FromPath(content::file_io_path(path));check(data.error()==Error::None,"cannot map material source");
    Parser parser(Extensions::KHR_texture_transform | Extensions::KHR_materials_emissive_strength | Extensions::KHR_materials_unlit | Extensions::EXT_meshopt_compression | Extensions::KHR_lights_punctual);
    MapLayerImport layers;layers.bind(parser);
    auto loaded=parser.loadGltf(data.get(),path.parent_path(),Options::None);
    check(loaded.error()==Error::None,"cannot parse material source");
    layers.validate();const auto& asset=loaded.get();octaryn::assets::validate_gltf_accessors(asset);
    check(validate(asset)==Error::None,"invalid material source");
    std::vector<bool> used(asset.images.size());MapModel result;
    check(!asset.scenes.empty(),"material source has no scene");
    const auto scene=asset.defaultScene.value_or(0);check(scene<asset.scenes.size(),"material scene out of range");
    result.environment=layers.environment(scene);
    for(const auto node:asset.scenes[scene].nodeIndices)walk_lights(asset,node,fmat4x4(),result,0);
    for(const auto& material:materials) {
      MapPrimitive primitive;primitive.material=material;result.primitives.push_back(primitive);
      for(const auto& texture:material.textures)if(texture.image>=0) {
        check(std::size_t(texture.image)<used.size(),"catalog material image reference invalid");used[texture.image]=true;
      }
    }
    octaryn::assets::GltfBufferViews buffers(path.parent_path(),limits.source_bytes,limits.cancel);
    load_images(asset,path.parent_path(),result,limits,buffers,&used);
    output=std::move(result);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
