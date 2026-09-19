#include "MapModel.h"
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
fmat4x4 node_matrix(const Node& node) {
  if(std::holds_alternative<TRS>(node.transform)) {
    const auto& trs=std::get<TRS>(node.transform);
    return scale(rotate(translate(fmat4x4(),trs.translation),trs.rotation),trs.scale);
  }
  return std::get<fmat4x4>(node.transform);
}
MapMaterial read_material(const Asset& asset,const Primitive& primitive) {
  check(primitive.materialIndex.has_value() && *primitive.materialIndex<asset.materials.size(),
      "map primitive material out of range");
  MapMaterial material;
  const auto& source=asset.materials[*primitive.materialIndex];
  for(size_t k=0;k<4;++k) material.base_color[k]=static_cast<float>(source.pbrData.baseColorFactor[k]);
  material.metallic=static_cast<float>(source.pbrData.metallicFactor);
  material.roughness=static_cast<float>(source.pbrData.roughnessFactor);
  material.alpha_mode=source.alphaMode==AlphaMode::Blend?MapAlphaMode::Blend:
      source.alphaMode==AlphaMode::Mask?MapAlphaMode::Mask:MapAlphaMode::Opaque;
  material.alpha_cutoff=material.alpha_mode==MapAlphaMode::Mask?static_cast<float>(source.alphaCutoff):0;
  material.double_sided=source.doubleSided;
  if(source.pbrData.baseColorTexture) {
    const auto texture_index=source.pbrData.baseColorTexture->textureIndex;
    check(texture_index<asset.textures.size(),"map texture index out of range");
    const auto& texture=asset.textures[texture_index];
    check(texture.imageIndex.has_value(),"map texture lacks an image (KTX2, DDS and WebP are unsupported)");
    check(*texture.imageIndex<asset.images.size(),"map image index out of range");
    material.texture=static_cast<std::int32_t>(*texture.imageIndex);
  }
  return material;
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
  const auto normal_matrix=transpose(inverse(mat<float,3,3>(world)));
  for(const auto& primitive:mesh.primitives) {
    check(model.primitives.size()<max_map_primitives,"too many map primitives");
    check(primitive.type==PrimitiveType::Triangles,"map requires triangle primitives");
    const auto positions=values<fvec3>(asset,attribute(primitive,"POSITION"),AccessorType::Vec3);
    const auto normals=values<fvec3>(asset,attribute(primitive,"NORMAL"),AccessorType::Vec3);
    const auto uvs=values<fvec2>(asset,attribute(primitive,"TEXCOORD_0"),AccessorType::Vec2);
    check(!positions.empty() && normals.size()==positions.size() && uvs.size()==positions.size(),
        "map vertex attribute count mismatch");
    // Non-indexed primitives render with sequential vertices per glTF.
    std::vector<std::uint32_t> indices;
    if(primitive.indicesAccessor.has_value())
      indices=values<std::uint32_t>(asset,*primitive.indicesAccessor,AccessorType::Scalar);
    else {
      indices.resize(positions.size());
      for(std::uint32_t i=0;i<positions.size();++i)indices[i]=i;
    }
    check(!indices.empty() && indices.size()%3==0,"map indices are not triangles");
    check(model.indices.size()+indices.size()<=max_map_triangles*3,"too many map triangles");
    check(model.vertices.size()+positions.size()<=0xFFFFFFFFu,"map vertex index overflow");
    MapPrimitive draw;
    draw.material=read_material(asset,primitive);
    draw.first_index=static_cast<std::uint32_t>(model.indices.size());
    draw.index_count=static_cast<std::uint32_t>(indices.size());
    for(size_t k=0;k<3;++k) {draw.bounds_min[k]=1e30f;draw.bounds_max[k]=-1e30f;}
    const auto vertex_base=static_cast<std::uint32_t>(model.vertices.size());
    model.vertices.resize(model.vertices.size()+positions.size());
    for(size_t i=0;i<positions.size();++i) {
      const auto position=world*fvec4(positions[i][0],positions[i][1],positions[i][2],1.f);
      const auto normal=normal_matrix*normals[i];
      MapVertex vertex{};
      for(size_t k=0;k<3;++k) {
        check(std::isfinite(position[k]) && std::isfinite(normal[k]),"nonfinite map vertex");
        vertex.position[k]=position[k];vertex.normal[k]=normal[k];
        draw.bounds_min[k]=std::min(draw.bounds_min[k],position[k]);
        draw.bounds_max[k]=std::max(draw.bounds_max[k],position[k]);
      }
      for(size_t k=0;k<2;++k) {check(std::isfinite(uvs[i][k]),"nonfinite map UV");vertex.uv[k]=uvs[i][k];}
      model.vertices[vertex_base+i]=vertex;
    }
    for(const auto index:indices) {
      check(index<positions.size(),"map vertex index out of range");
      model.indices.push_back(index+vertex_base);
    }
    model.primitives.push_back(std::move(draw));
  }
}
void walk_nodes(const Asset& asset,size_t index,const fmat4x4& parent,MapModel& model,size_t depth) {
  check(depth<=1024 && index<asset.nodes.size(),"invalid map node hierarchy");
  const auto& node=asset.nodes[index];
  check(!node.skinIndex,"map nodes must be static (skinned nodes are unsupported)");
  const fmat4x4 world=parent*node_matrix(node);
  if(node.meshIndex)add_mesh(asset,asset.meshes[*node.meshIndex],world,model);
  for(const auto child:node.children)walk_nodes(asset,child,world,model,depth+1);
}
}
bool load_map_model(const std::filesystem::path& path,MapModel& output,std::string& error) {
  try {
    check(std::filesystem::file_size(path)<=max_map_file_bytes,"map asset exceeds size bound");
    auto data=GltfDataBuffer::FromPath(path);check(data.error()==Error::None,"cannot read map asset");
    Parser parser;
    auto loaded=parser.loadGltf(data.get(),path.parent_path(),Options::LoadExternalBuffers);
    check(loaded.error()==Error::None,"cannot parse map glTF");
    const auto& asset=loaded.get();check(validate(asset)==Error::None,"invalid map glTF");
    check(!asset.scenes.empty(),"map has no scene");
    const auto scene=asset.defaultScene.value_or(0);
    check(scene<asset.scenes.size(),"map default scene out of range");
    MapModel result;
    load_images(asset,path.parent_path(),result);
    for(const auto node:asset.scenes[scene].nodeIndices)walk_nodes(asset,node,fmat4x4(),result,0);
    check(!result.primitives.empty(),"map scene has no primitives");
    output=std::move(result);error.clear();return true;
  } catch(const std::exception& exception) {error=exception.what();return false;}
}
}
