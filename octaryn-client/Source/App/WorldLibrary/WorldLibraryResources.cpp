#include "WorldLibraryRecords.h"
#include "WorldLibraryIo.h"
#include "MapTextureCache.h"
#include "SessionFiles.h"
#include <fastgltf/core.hpp>
#include <glaze/glaze.hpp>
#include <cmath>
#include <set>

namespace octaryn::client::app {
namespace {
bool resource_metadata(const std::filesystem::path& path,WorldSourceResource& output) {
  std::error_code ec;
  const auto canonical=std::filesystem::weakly_canonical(path,ec);
  if(ec || !std::filesystem::is_regular_file(canonical,ec))return false;
  const auto bytes=std::filesystem::file_size(canonical,ec);if(ec)return false;
  const auto modified=std::filesystem::last_write_time(canonical,ec);if(ec)return false;
  output={world_library_utf8(canonical),{},bytes,int64_t(modified.time_since_epoch().count())};return true;
}
bool gather_resources(const std::filesystem::path& source,std::set<std::filesystem::path>& paths,std::string& error) {
  paths.insert(source);
  world_library_note_io(WorldLibraryIo::SourceParse);
  auto data=fastgltf::MappedGltfFile::FromPath(source);
  if(data.error()!=fastgltf::Error::None) {error="World source could not be read.";return false;}
  fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform | fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_unlit | fastgltf::Extensions::EXT_meshopt_compression);
  auto loaded=parser.loadGltf(data.get(),source.parent_path(),fastgltf::Options::None);
  if(loaded.error()!=fastgltf::Error::None) {error="World resource references could not be read.";return false;}
  const auto add=[&](const fastgltf::DataSource& resource) {
    if(const auto* uri=std::get_if<fastgltf::sources::URI>(&resource))paths.insert(source.parent_path()/uri->uri.fspath());
  };
  for(const auto& buffer:loaded.get().buffers)add(buffer.data);
  for(const auto& image:loaded.get().images)add(image.data);
  return true;
}
}
bool world_library_authored_tiles(const std::filesystem::path& manifest,WorldAuthoredTiles& result,std::string& error) {
  if(manifest.empty())return true;
  std::string text;
  constexpr glz::opts options{.error_on_unknown_keys=false};
  if(!local_session::read_text(manifest,text,1024u*1024) || glz::read<options>(result,text) ||
      result.tile_files.size()!=result.tiles.size() || result.tile_files.size()>65536) {error="Authored tile manifest is invalid.";return false;}
  for(size_t index=0;index<result.tile_files.size();++index) {
    const auto path=world_library_path(result.tile_files[index]);
    bool valid=!path.empty() && path.is_relative() && !path.has_root_name();
    for(const auto& part:path)valid=valid && part!="..";
    for(float value:result.tiles[index])valid=valid && std::isfinite(value);
    for(unsigned axis=0;axis<3;++axis)valid=valid && result.tiles[index][axis]<=result.tiles[index][axis+3];
    if(!valid) {error="Authored tile references are invalid.";return false;}
  }
  return true;
}
bool world_library_resources(const std::filesystem::path& source,WorldLibraryRecord& record,std::string& error,const std::atomic_bool* cancel) {
  const auto canceled=[&] {if(cancel && cancel->load()) {error="World import canceled.";return true;}return false;};
  if(canceled())return false;
  std::set<std::filesystem::path> paths;
  if(!gather_resources(source,paths,error))return false;
  WorldAuthoredTiles authored;
  if(!world_library_authored_tiles(world_library_path(record.manifest),authored,error))return false;
  if(!record.manifest.empty())paths.insert(world_library_path(record.manifest));
  for(const auto& tile:authored.tile_files) {
    if(canceled())return false;
    if(!gather_resources(world_library_path(record.manifest).parent_path()/world_library_path(tile),paths,error))return false;
    if(paths.size()>4096) {error="This world references too many resources.";return false;}
  }
  if(paths.size()>4096) {error="This world references too many resources.";return false;}
  record.resources.clear();
  for(const auto& path:paths) {
    if(canceled())return false;
    WorldSourceResource resource;
    if(!resource_metadata(path,resource)) {error="A world buffer, texture or manifest is missing.";return false;}
    world_library_note_io(WorldLibraryIo::ResourceHash);
    resource.digest=rendering::map_texture_file_digest(path,error,64ull*1024*1024*1024,cancel);
    if(resource.digest.empty())return false;
    record.resources.push_back(std::move(resource));
  }
  if(record.name.empty() || record.name.size()>256) {error="World filename is too long.";return false;}
  return true;
}
bool world_library_resources_current(const WorldLibraryRecord& record,const std::atomic_bool* cancel) {
  if(record.resources.empty())return false;
  for(const auto& expected:record.resources) {
    if(cancel && cancel->load())return false;
    WorldSourceResource current;
    if(!resource_metadata(world_library_path(expected.path),current) || current.bytes!=expected.bytes)return false;
    if(current.modified!=expected.modified) {
      std::string error;
      world_library_note_io(WorldLibraryIo::ResourceHash);
      if(rendering::map_texture_file_digest(world_library_path(expected.path),error,64ull*1024*1024*1024,cancel)!=expected.digest)return false;
    }
  }
  return true;
}
}
