#include "AnimationCook.h"
#include "../MapWorld/MapMaterials.h"
#include "../MapWorld/MapTextureCache.h"
#include <fastgltf/core.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>

namespace octaryn::client::animation {
namespace {
void require(bool valid,const char* error) {if(!valid)throw std::runtime_error(error);}
std::span<const std::byte> bytes(const fastgltf::DataSource& source) {
  using namespace fastgltf;
  if(const auto* data=std::get_if<sources::Array>(&source))return {data->bytes.data(),data->bytes.size()};
  if(const auto* data=std::get_if<sources::ByteView>(&source))return data->bytes;
  if(const auto* data=std::get_if<sources::Vector>(&source))return {data->bytes.data(),data->bytes.size()};
  return {};
}
rendering::MapModelImage image(const fastgltf::Asset& asset,const fastgltf::Image& source,const std::filesystem::path& parent) {
  using namespace fastgltf;rendering::MapModelImage output;std::span<const std::byte> data;
  MimeType mime=MimeType::None;
  if(const auto* view=std::get_if<sources::BufferView>(&source.data)) {
    const auto& info=asset.bufferViews.at(view->bufferViewIndex);const auto buffer=bytes(asset.buffers.at(info.bufferIndex).data);
    require(info.byteOffset<=buffer.size() && info.byteLength<=buffer.size()-info.byteOffset,"animated image buffer range invalid");
    data=buffer.subspan(info.byteOffset,info.byteLength);mime=view->mimeType;
  }else if(const auto* uri=std::get_if<sources::URI>(&source.data)) {
    const auto path=parent/uri->uri.fspath();const auto size=std::filesystem::file_size(path);
    require(uri->fileByteOffset<=size && size-uri->fileByteOffset<=86ull*1024*1024,"animated external image exceeds limit");
    output.bytes.resize(size_t(size-uri->fileByteOffset));std::ifstream file(path,std::ios::binary);
    file.seekg(std::streamoff(uri->fileByteOffset));require(bool(file.read(reinterpret_cast<char*>(output.bytes.data()),output.bytes.size())),"animated image read failed");
    mime=uri->mimeType;
    if(mime==MimeType::None) {
      const auto extension=path.extension();output.mime_type=extension==".png"?"image/png":
          extension==".jpg" || extension==".jpeg"?"image/jpeg":"";
    }
  }else {
    data=bytes(source.data);
    if(const auto* array=std::get_if<sources::Array>(&source.data))mime=array->mimeType;
    if(const auto* view=std::get_if<sources::ByteView>(&source.data))mime=view->mimeType;
  }
  if(!data.empty()) {
    require(data.size()<=86ull*1024*1024,"animated image exceeds limit");output.bytes.resize(data.size());
    std::memcpy(output.bytes.data(),data.data(),data.size());
  }
  if(mime!=MimeType::None)output.mime_type=std::string(getMimeTypeString(mime));
  require(!output.bytes.empty() && (output.mime_type=="image/png" || output.mime_type=="image/jpeg"),"animated image requires PNG or JPEG");
  return output;
}
}
bool cook_animation(const std::filesystem::path& path,CookedAsset& output,std::string& error) {
  try {
    CookedAsset cooked;if(!load_asset(path,cooked.animation,error))return false;
    using namespace fastgltf;auto data=MappedGltfFile::FromPath(path);require(data.error()==Error::None,"animated source map failed");
    Parser parser(Extensions::KHR_texture_transform|Extensions::KHR_materials_emissive_strength|Extensions::KHR_materials_unlit);
    auto loaded=parser.loadGltf(data.get(),path.parent_path(),Options::LoadExternalBuffers);
    require(loaded.error()==Error::None,"animated material source parse failed");const auto& source=loaded.get();
    require(validate(source)==Error::None,"animated material source invalid");
    for(const auto& primitive:cooked.animation.primitives) {
      fastgltf::Primitive material;
      if(primitive.material>=0)material.materialIndex=size_t(primitive.material);
      cooked.materials.push_back(rendering::load_map_material(source,material));
    }
    require(source.images.size()<=1024,"animated image count limit");std::uint64_t image_bytes=0;
    for(const auto& item:source.images) {
      auto decoded=image(source,item,path.parent_path());image_bytes+=decoded.bytes.size();
      require(image_bytes<=512ull*1024*1024,"animated image aggregate limit");cooked.images.push_back(std::move(decoded));
    }
    const auto source_hash=rendering::map_texture_file_digest(path,error);if(source_hash.empty())return false;
    std::vector<std::span<const std::uint8_t>> fingerprint;
    fingerprint.emplace_back(reinterpret_cast<const std::uint8_t*>(source_hash.data()),source_hash.size());
    for(const auto& buffer:source.buffers) {
      const auto payload=bytes(buffer.data);require(payload.size()>=buffer.byteLength,"animated buffer not resident");
      fingerprint.emplace_back(reinterpret_cast<const std::uint8_t*>(payload.data()),size_t(buffer.byteLength));
    }
    for(const auto& image:cooked.images)fingerprint.emplace_back(image.bytes);
    const auto hash=rendering::map_texture_digest_parts(fingerprint);
    if(!build_animation_geometry(cooked,hash,error))return false;
    output=std::move(cooked);error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
