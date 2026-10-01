#include "WorldLibrary.h"
#include "WorldLibraryRecords.h"
#include "WorldLibraryIo.h"
#include "MapManifest.h"
#include "SceneCatalog.h"
#include "GeometryCache.h"
#include "SceneHierarchy.h"
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;
namespace vg=octaryn::client::rendering::virtual_geometry;
void require(bool valid,const std::string& reason) {if(!valid)throw std::runtime_error(reason);}
std::string read(const fs::path& path) {std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
void write(const fs::path& path,const std::string& text) {
  fs::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary);file<<text;require(bool(file),"prepare fixture write failed");
}
}
void test_world_library_prepare(const std::filesystem::path& original,const std::filesystem::path& root) {
  using namespace octaryn::client::app;
  const auto source=root/"sources"/"first.glb",other=root/"sources"/"second.glb";
  const auto bytes=read(original);require(!bytes.empty(),"prepare fixture source missing");write(source,bytes);write(other,bytes);
  const auto state=root/"state",directory=state/"saves"/"worlds";
  WorldLibrary library(state,root/"empty-bundle");std::string error;
  require(library.add_source(source,error) && library.add_source(other,error) && library.entries().size()==2,error);
  const auto first=library.entries()[0].id,second=library.entries()[1].id;
  std::atomic_bool cancel{true};
  require(!library.prepare(first,error,&cancel) && !fs::exists(directory/"prepared"/first/"prepared.json"),"canceled preparation published a ready world");
  require(library.entries()[0].saves.empty() && library.entries()[1].saves.empty(),"preparation created a save");
  cancel=false;unsigned updates{};
  require(library.prepare(first,error,&cancel,[&](const auto&){++updates;}) && updates,error);
  WorldLibraryCatalog catalog;require(world_library_load_catalog(directory/"catalog.json",catalog,error),error);
  const auto found=std::find_if(catalog.worlds.begin(),catalog.worlds.end(),[&](const auto& world){return world.id==first;});
  require(found!=catalog.worlds.end(),"prepared world identity disappeared");MapManifest manifest;
  require(world_library_prepared_scene(directory,*found,manifest) && !manifest.scene_catalog.empty() && fs::equivalent(manifest.glb,source),
      "prepared scene lost original source identity");
  const auto scene=manifest.scene_catalog;const auto scene_text=read(scene);
  vg::SceneCatalog geometry;
  require(vg::read_scene_catalog(scene,geometry,error),error);
  require(!manifest.scene_hierarchy.empty(),"new preparation omitted complete hierarchy");
  vg::SceneHierarchy hierarchy;
  require(vg::read_scene_hierarchy(manifest.scene_hierarchy,hierarchy,error) && hierarchy.complete,error);
  for(const auto& primitive:hierarchy.primitives)for(const auto& root:primitive.roots) {
    vg::GeometryAsset asset;
    fs::path path;require(vg::scene_hierarchy_path(manifest.scene_hierarchy,root.coarse.file,path,error),error);
    require(vg::read_geometry_cache(path,root.coarse.hash,asset,error,false),error);
    for(const auto& page:asset.pages) {
      std::vector<std::uint8_t> decoded;
      require(vg::read_geometry_page(path,page,decoded,error) && !decoded.empty(),error);
    }
  }
  const auto hierarchy_path=manifest.scene_hierarchy;const auto hierarchy_text=read(hierarchy_path);
  write(hierarchy_path,hierarchy_text+" ");
  require(!world_library_prepared_scene(directory,*found,manifest),"prepared hierarchy seal accepted changed metadata");
  write(hierarchy_path,hierarchy_text);
  write(scene,scene_text+" ");
  require(!world_library_prepared_scene(directory,*found,manifest),"prepared catalog seal accepted changed metadata");
  write(scene,scene_text);
  world_library_reset_io_stats();
  WorldLibrary restarted(state,root/"empty-bundle");require(restarted.refresh(error),error);
  const auto listed=world_library_io_stats();
  require(!listed.source_parses && !listed.resource_hashes && !listed.model_loads && !listed.prepared_catalog_reads,
      "listing prepared world inspected source or prepared catalog content");
  fs::path saved,new_save,other_save;
  require(restarted.open_world(first,saved,error) && WorldLibrary::resolve(saved,manifest,error),error);
  require(fs::equivalent(manifest.glb,source) && fs::equivalent(manifest.scene_catalog,scene),"save did not resolve prepared scene");
  write(saved/"client"/"inventory.json","first-progress");
  require(restarted.new_save(first,new_save,error) && restarted.open_world(second,other_save,error),error);
  require(saved!=new_save && saved.parent_path()!=other_save.parent_path() && !fs::exists(new_save/"client"/"inventory.json") &&
      !fs::exists(other_save/"client"/"inventory.json"),"prepared world progress crossed save boundaries");
  write(source,bytes+" ");require(!WorldLibrary::resolve(saved,manifest,error),"prepared source mutation reused prior save");
  write(source,bytes);require(WorldLibrary::resolve(saved,manifest,error),error);
  require(read(source)==bytes && read(other)==bytes,"preparation modified source assets");
  std::puts("world_library_prepare_flow=passed canceled=1 resumed=1 catalog_seal=1 original_source=1 restart_identity=1 isolated_saves=1 source_mutation_rejected=1 geometry_page_reads=1 prepared_menu_reads=0");
}
