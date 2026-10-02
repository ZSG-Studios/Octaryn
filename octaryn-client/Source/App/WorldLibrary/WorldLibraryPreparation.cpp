#include "WorldLibraryRecords.h"
#include "WorldLibraryIo.h"
#include "MapSource.h"
#include "GltfBufferViews.h"
#include <fastgltf/core.hpp>
#include <map>
#include <mutex>
#include <string_view>
#include <exception>

namespace octaryn::client::app {
bool world_library_preparation_required(const std::filesystem::path& source) {
  struct Entry {std::uintmax_t bytes{};std::filesystem::file_time_type modified;bool required{};};
  static std::mutex mutex;
  static std::map<std::filesystem::path,Entry> inspected;
  std::error_code ec;
  const auto bytes=std::filesystem::file_size(source,ec);if(ec)return false;
  const auto modified=std::filesystem::last_write_time(source,ec);if(ec)return false;
  std::lock_guard lock(mutex);
  if(const auto found=inspected.find(source);found!=inspected.end() &&
      found->second.bytes==bytes && found->second.modified==modified)return found->second.required;
  bool required=false;
  try {
    if(bytes>rendering::MapLoadLimits{}.source_bytes)required=true;
    else {
      world_library_note_io(WorldLibraryIo::SourceParse);
      auto data=fastgltf::MappedGltfFile::FromPath(source);
      if(data.error()==fastgltf::Error::None) {
        fastgltf::Parser parser(fastgltf::Extensions::EXT_meshopt_compression |
            fastgltf::Extensions::KHR_texture_transform | fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_unlit);
        auto asset=parser.loadGltf(data.get(),source.parent_path(),fastgltf::Options::None);
        if(asset.error()==fastgltf::Error::None) {
          octaryn::assets::validate_gltf_accessors(asset.get());
          if(fastgltf::validate(asset.get())==fastgltf::Error::None) {
            try {rendering::qualify_map_scene(asset.get(),{},false);}
            catch(const std::exception& failure) {
              required=std::string_view(failure.what()).starts_with("This scene requires a streamed instance cook:");
            }
          }
        }
      }
    }
  } catch(const std::exception&) {}
  if(inspected.size()>=4096)inspected.clear();
  inspected[source]={bytes,modified,required};return required;
}
}
