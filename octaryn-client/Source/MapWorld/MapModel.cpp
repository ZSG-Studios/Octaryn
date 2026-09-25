#include "MapModel.h"
#include "MapMaterials.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>

namespace octaryn::client::rendering {
namespace {
using namespace fastgltf;
using namespace fastgltf::math;
constexpr std::uint64_t max_map_file_bytes=512ull*1024*1024;
constexpr std::size_t max_map_triangles=8000000,max_map_primitives=4096,max_map_images=1024;
void check(bool value,const char* reason) { if(!value) throw std::runtime_error(reason); }
template<class T> std::vector<T> values(const Asset& asset,size_t index,AccessorType type) {
  check(index<asset.accessors.size(),"accessor index out of range");
  const auto& accessor=asset.accessors[index];
  check(accessor.type==type && accessor.count<=24000000,"unsupported accessor shape or size");
  std::vector<T> result;result.reserve(accessor.count);
  iterateAccessor<T>(asset,accessor,[&](T value) {result.push_back(value);});
  return result;
}
size_t attribute(const Primitive& primitive,const char* name) {
  const auto found=primitive.findAttribute(name);
  check(found!=primitive.attributes.end(),"missing map vertex attribute");
  return found->accessorIndex;
}
template<class T> std::vector<T> optional_values(const Asset& asset,const Primitive& primitive,
    const char* name,AccessorType type,size_t count) {
  const auto found=primitive.findAttribute(name);
  if(found==primitive.attributes.end())return {};
  auto result=values<T>(asset,found->accessorIndex,type);
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
    size_t vertex_count,size_t remaining_indices,bool mirrored) {
  check(primitive.type==PrimitiveType::Triangles || primitive.type==PrimitiveType::TriangleStrip ||
      primitive.type==PrimitiveType::TriangleFan,"map requires triangle, strip or fan primitives");
  std::vector<std::uint32_t> source;
  if(primitive.indicesAccessor)source=values<std::uint32_t>(asset,*primitive.indicesAccessor,AccessorType::Scalar);
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
std::span<const std::byte> buffer_bytes(const DataSource& data) {
  if(const auto* array=std::get_if<sources::Array>(&data))
    return std::span<const std::byte>(array->bytes.data(),array->bytes.size());
  if(const auto* view=std::get_if<sources::ByteView>(&data)) return view->bytes;
  if(const auto* vector=std::get_if<sources::Vector>(&data))
    return std::span<const std::byte>(vector->bytes.data(),vector->bytes.size());
  return {};
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
void load_image_bytes(std::span<const std::byte> source,MimeType mime,MapModelImage& target) {
  target.bytes.resize(source.size());
  std::memcpy(target.bytes.data(),source.data(),source.size());
  if(mime!=MimeType::None)target.mime_type=std::string(getMimeTypeString(mime));
}
void load_images(const Asset& asset,const std::filesystem::path& parent,MapModel& model) {
  check(asset.images.size()<=max_map_images,"too many map images");
  model.images.resize(asset.images.size());
  for(size_t index=0;index<asset.images.size();++index) {
    auto& target=model.images[index];
    const auto& data=asset.images[index].data;
    if(const auto* view=std::get_if<sources::BufferView>(&data)) {
      check(view->bufferViewIndex<asset.bufferViews.size(),"map image buffer view out of range");
      const auto& buffer_view=asset.bufferViews[view->bufferViewIndex];
      check(buffer_view.bufferIndex<asset.buffers.size(),"map image buffer out of range");
      const auto bytes=buffer_bytes(asset.buffers[buffer_view.bufferIndex].data);
      check(bytes.size()>=buffer_view.byteOffset+buffer_view.byteLength,"map image exceeds its buffer");
      check_supported_mime(mime_string(view->mimeType,nullptr));
      load_image_bytes(bytes.subspan(buffer_view.byteOffset,buffer_view.byteLength),view->mimeType,target);
    } else if(const auto* uri=std::get_if<sources::URI>(&data)) {
      check_supported_mime(mime_string(uri->mimeType,uri));
      target.mime_type=mime_string(uri->mimeType,uri);
      std::ifstream file(parent/uri->uri.fspath(),std::ios::binary);
      check(file.good(),"cannot read external map image");
      file.seekg(static_cast<std::streamoff>(uri->fileByteOffset));
      std::vector<char> encoded((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
      check(!encoded.empty(),"empty external map image");
      target.bytes.resize(encoded.size());
      std::memcpy(target.bytes.data(),encoded.data(),encoded.size());
    } else if(const auto* array=std::get_if<sources::Array>(&data)) {
      check_supported_mime(mime_string(array->mimeType,nullptr));
      load_image_bytes(std::span<const std::byte>(array->bytes.data(),array->bytes.size()),array->mimeType,target);
    } else if(const auto* byte_view=std::get_if<sources::ByteView>(&data)) {
      check_supported_mime(mime_string(byte_view->mimeType,nullptr));
      load_image_bytes(byte_view->bytes,byte_view->mimeType,target);
    } else check(false,"unsupported map image source");
  }
}
void add_mesh(const Asset& asset,const Mesh& mesh,const fmat4x4& world,MapModel& model) {
  const auto determinant_value=determinant(mat<float,3,3>(world));
  check(std::isfinite(determinant_value) && determinant_value!=0.f,"singular or nonfinite map transform");
  const auto normal_matrix=transpose(inverse(mat<float,3,3>(world)));
  for(const auto& primitive:mesh.primitives) {
    check(primitive.targets.empty(),"map nodes must be static (morph targets are unsupported)");
    check(model.primitives.size()<max_map_primitives,"too many map primitives");
    const auto positions=values<fvec3>(asset,attribute(primitive,"POSITION"),AccessorType::Vec3);
    check(!positions.empty(),"empty map primitive");
    const auto normals=optional_values<fvec3>(asset,primitive,"NORMAL",AccessorType::Vec3,positions.size());
    const auto uvs=optional_values<fvec2>(asset,primitive,"TEXCOORD_0",AccessorType::Vec2,positions.size());
    const auto uv1=optional_values<fvec2>(asset,primitive,"TEXCOORD_1",AccessorType::Vec2,positions.size());
    const auto tangents=optional_values<fvec4>(asset,primitive,"TANGENT",AccessorType::Vec4,positions.size());
    std::vector<fvec4> colors;
    if(const auto color=primitive.findAttribute("COLOR_0");color!=primitive.attributes.end()) {
      check(color->accessorIndex<asset.accessors.size(),"map color accessor out of range");
      if(asset.accessors[color->accessorIndex].type==AccessorType::Vec3) {
        const auto rgb=values<fvec3>(asset,color->accessorIndex,AccessorType::Vec3);
        for(const auto& value:rgb)colors.emplace_back(value[0],value[1],value[2],1.f);
      } else colors=values<fvec4>(asset,color->accessorIndex,AccessorType::Vec4);
      check(colors.size()==positions.size(),"map color count mismatch");
    }
    auto indices=triangle_indices(asset,primitive,positions.size(),max_map_triangles*3-model.indices.size(),
        determinant_value<0.f);
    const auto vertex_count=normals.empty()?indices.size():positions.size();
    check(vertex_count<=max_map_triangles*3-model.vertices.size(),"too many map vertices");
    MapPrimitive draw;
    draw.material=load_map_material(asset,primitive);
    for(const auto& texture:draw.material.textures)if(texture.image>=0)
      check(texture.texcoord==0?!uvs.empty():!uv1.empty(),"map texture references a missing UV attribute");
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
      for(size_t k=0;k<4;++k) {
        const auto value=colors.empty()?1.f:colors[i][k];
        check(std::isfinite(value),"nonfinite map vertex color");vertex.color[k]=value;
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
    model.primitives.push_back(std::move(draw));
  }
}
void walk_nodes(const Asset& asset,size_t index,const fmat4x4& parent,MapModel& model,size_t depth,bool catalog) {
  check(depth<=1024 && index<asset.nodes.size(),"invalid map node hierarchy");
  const auto& node=asset.nodes[index];
  check(!node.skinIndex,"map nodes must be static (skinned nodes are unsupported)");
  const fmat4x4 world=parent*node_matrix(node);
  if(node.meshIndex) {
    check(*node.meshIndex<asset.meshes.size(),"map mesh index out of range");
    if(catalog) {
      for(const auto& primitive:asset.meshes[*node.meshIndex].primitives) {
        check(primitive.targets.empty(),"map nodes must be static (morph targets are unsupported)");
        check(model.primitives.size()<max_map_primitives,"too many map primitives");
        MapPrimitive entry;entry.material=load_map_material(asset,primitive);model.primitives.push_back(entry);
      }
    } else add_mesh(asset,asset.meshes[*node.meshIndex],world,model);
  }
  for(const auto child:node.children)walk_nodes(asset,child,world,model,depth+1,catalog);
}
bool load_map_asset(const std::filesystem::path& path,MapModel& output,std::string& error,bool catalog) {
  try {
    check(std::filesystem::file_size(path)<=max_map_file_bytes,"map asset exceeds size bound");
    auto data=MappedGltfFile::FromPath(path);check(data.error()==Error::None,"cannot map map asset");
    Parser parser(Extensions::KHR_texture_transform | Extensions::KHR_materials_emissive_strength);
    auto loaded=parser.loadGltf(data.get(),path.parent_path(),Options::LoadExternalBuffers);
    check(loaded.error()==Error::None,"cannot parse map glTF");
    const auto& asset=loaded.get();check(validate(asset)==Error::None,"invalid map glTF");
    check(!asset.scenes.empty(),"map has no scene");
    const auto scene=asset.defaultScene.value_or(0);
    check(scene<asset.scenes.size(),"map default scene out of range");
    MapModel result;
    load_images(asset,path.parent_path(),result);
    for(const auto node:asset.scenes[scene].nodeIndices)walk_nodes(asset,node,fmat4x4(),result,0,catalog);
    check(!result.primitives.empty(),"map scene has no primitives");
    output=std::move(result);error.clear();return true;
  } catch(const std::exception& exception) {error=exception.what();return false;}
}
}
bool load_map_model(const std::filesystem::path& path,MapModel& output,std::string& error) {
  return load_map_asset(path,output,error,false);
}
bool load_map_texture_catalog(const std::filesystem::path& path,MapModel& output,std::string& error) {
  return load_map_asset(path,output,error,true);
}
}
