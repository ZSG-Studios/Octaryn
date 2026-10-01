#include "WorldLibrary.h"
#include "MapManifest.h"
#include "WorldLibraryRecords.h"
#include "WorldLibraryIo.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs=std::filesystem;
using namespace octaryn::client::app;
void test_world_library_prepare(const fs::path&,const fs::path&);
void test_world_library_lazy(const fs::path&);
void test_world_library_registration(const fs::path&,const fs::path&);
namespace {
void require(bool valid,const std::string& message) {if(!valid)throw std::runtime_error(message);}
std::string contents(const fs::path& path) {std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
void write(const fs::path& path,const std::string& value) {fs::create_directories(path.parent_path());std::ofstream(path,std::ios::binary)<<value;}
void validate_migration(const fs::path& root,const fs::path& sources) {
  const auto shared=root/"shared-sources";
  write(shared/"main.glb",contents(sources/"authored"/"main.glb"));
  write(shared/"map.json",contents(sources/"authored"/"map.json"));
  write(shared/"unrelated.glb",contents(sources/"1"/"main.glb"));
  write(shared/"player_1.json","shared-player");write(shared/"world_time.json","shared-clock");
  write(shared/"world-state.save","shared-authority");write(shared/"client"/"inventory.json","shared-inventory");
  WorldLibrary library(root/"shared-state",root/"empty-bundle");std::string error;
  require(library.add_source(shared/"main.glb",error),error);
  require(library.add_source(shared/"unrelated.glb",error),error);
  fs::path authored,raw;
  require(library.open_world(library.entries()[0].id,authored,error),error);
  require(contents(authored/"world-state.save")=="shared-authority" &&
      contents(authored/"client"/"inventory.json")=="shared-inventory","authored migration lost progress");
  require(library.open_world(library.entries()[1].id,raw,error),error);
  for(const auto& relative:{"player_1.json","world_time.json","world-state.save","client/inventory.json"})
    require(!fs::exists(raw/relative),"raw world inherited unrelated neighboring save");
  for(unsigned scenario=0;scenario<3;++scenario) {
    const auto app=root/("legacy-"+std::to_string(scenario));const auto bundle=app/"bundle";
    const auto current=app/"octaryn-client"/"Assets"/"Maps";const auto legacy=bundle/"Client"/"Assets"/"Maps";
    write(current/"main.glb",contents(sources/"authored"/"main.glb"));
    write(current/"map.json",contents(sources/"authored"/"map.json"));
    write(legacy/"main.glb",contents(sources/(scenario==1?"1":"authored")/"main.glb"));
    auto legacy_manifest=contents(current/"map.json");
    if(scenario==2) {
      write(legacy/"other.glb",contents(current/"main.glb"));
      legacy_manifest.replace(legacy_manifest.find("main.glb"),8,"other.glb");
    } else if(scenario==0) {
      // Packaging may adjust the spawn metadata while retaining the world assets.
      legacy_manifest.replace(legacy_manifest.find("-9"),2,"-8");
    }
    write(legacy/"map.json",legacy_manifest);
    write(legacy/"world-state.save","bundled-progress");
    write(legacy/"client"/"inventory.json","bundled-inventory");
    WorldLibrary builtin(app,bundle);require(builtin.refresh(error),error);
    require(builtin.entries().size()==1,"builtin legacy fixture registration");
    fs::path saved;require(builtin.open_world(builtin.entries().front().id,saved,error),error);
    if(scenario==0)require(contents(saved/"world-state.save")=="bundled-progress" &&
        contents(saved/"client"/"inventory.json")=="bundled-inventory","matching builtin progress not migrated");
    else require(!fs::exists(saved/"world-state.save") && !fs::exists(saved/"client"/"inventory.json"),
        "unrelated legacy bundle progress migrated");
  }
}
}
int main(int argc,char** argv) {
  try {
    if(argc==4 && std::string(argv[1])=="--register-only") {
      test_world_library_registration(fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[2]))),
          fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[3]))));return 0;
    }
    if(argc==3 && std::string(argv[1])=="--lazy") {
      test_world_library_lazy(fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[2]))));return 0;
    }
    if(argc==4 && std::string(argv[1])=="--prepare-flow") {
      test_world_library_prepare(fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[2]))),
          fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[3]))));return 0;
    }
    if(argc==4 && std::string(argv[1])=="--preparation") {
      const auto source=fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[2])));
      const auto root=fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[3])));
      WorldLibrary library(root,root/"empty-bundle");std::string error;fs::path saved;
      require(library.add_source(source,error) && library.entries().size()==1,error);
      const auto id=library.entries().front().id;
      require(!library.open_world(id,saved,error) && error.find("prepar")!=std::string::npos,"large source did not discover preparation on selection");
      const auto entry=library.entries().front();
      require(entry.preparation_required && !entry.available && entry.saves.empty(),"large source advertised as ready");
      require(!library.open_world(entry.id,saved,error) && error.find("preparation")!=std::string::npos,"unprepared source opened");
      require(!library.new_save(entry.id,saved,error),"unprepared source created a save");
      require(!fs::exists(root/"saves"/"worlds"/entry.id),"unprepared source wrote save state");
      WorldLibrary restarted(root,root/"empty-bundle");
      require(restarted.refresh(error) && restarted.entries().size()==1 && restarted.entries().front().id==entry.id &&
          restarted.entries().front().preparation_required,"preparation state lost source identity on restart");
      std::printf("world_library_preparation=passed recognized=1 ready=0 saves_created=0 restart_identity=1\n");return 0;
    }
    if(argc==4 && std::string(argv[1])=="--import") {
      const auto source=fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[2])));
      const auto root=fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[3])));
      WorldLibrary library(root,root/"empty-bundle");std::string error;fs::path saved;
      require(library.add_source(source,error),error);
      require(library.refresh(error) && library.entries().size()==1,error);
      require(library.open_world(library.entries().front().id,saved,error),error);
      MapManifest manifest;require(WorldLibrary::resolve(saved,manifest,error),error);
      require(fs::equivalent(source,manifest.glb),"import changed source identity");
      std::printf("world_library_import=passed worlds=1 collision_spawn=1 resources=1 source=%s save=%s\n",
          source.string().c_str(),saved.string().c_str());return 0;
    }
    if(argc==4 && std::string(argv[1])=="--migrate-bundle") {
      WorldLibrary library(fs::path(reinterpret_cast<const char8_t*>(argv[2])),fs::path(reinterpret_cast<const char8_t*>(argv[3])));
      std::string error;require(library.refresh(error),error);
      std::vector<std::string> ids;for(const auto& entry:library.entries())if(entry.available)ids.push_back(entry.id);
      for(const auto& id:ids) {fs::path saved;require(library.open_world(id,saved,error),error);}
      std::printf("world_library_migration=passed worlds=%zu source_assets_unchanged=1\n",ids.size());return 0;
    }
    require(argc==2,"usage: world_library_probe fixture_directory");
    const auto root=fs::absolute(fs::path(reinterpret_cast<const char8_t*>(argv[1])));const auto sources=root/"sources";
    validate_migration(root,sources);
    write(sources/"0"/"map.json","{\"version\":1,\"map\":\"main.glb\",\"spawn\":[0,1.62,0],\"yaw\":0,\"pitch\":0}");
    WorldLibrary library(root/"state",root/"empty-bundle");std::string error;
    require(library.refresh(error),error);
    for(unsigned index=0;index<5;++index)require(library.add_source(sources/std::to_string(index)/"main.glb",error),error);
    require(library.add_source(sources/"external"/"scene.gltf",error),error);
    require(library.entries().size()==6,"library still restricted to three worlds");
    const auto first=library.entries().front();
    require(library.add_source(sources/"0"/"main.glb",error) && library.entries().size()==6,"duplicate import changed identity");
    const auto before=contents(sources/"0"/"main.glb");
    write(sources/"0"/"player_1.json","authoritative-player");
    write(sources/"0"/"client"/"inventory.json","existing-inventory");
    fs::path old_save;
    require(library.open_world(first.id,old_save,error),error);
    require(contents(old_save/"player_1.json")=="authoritative-player" &&
        contents(old_save/"client"/"inventory.json")=="existing-inventory","first open lost previous save");
    MapManifest manifest;
    require(WorldLibrary::resolve(old_save,manifest,error),error);
    require(fs::equivalent(manifest.glb,sources/"0"/"main.glb"),"resolver points at bundled world");
    require(std::isfinite(manifest.spawn_y) && std::abs(manifest.spawn_y-1.62f)<.1f,"generated spawn is not on floor");
    fs::path new_save;
    require(library.new_save(first.id,new_save,error),error);
    require(new_save!=old_save && !fs::exists(new_save/"player_1.json") && fs::exists(old_save/"player_1.json"),"new save overwrote earlier progress");
    write(new_save/"client"/"inventory.json","new-inventory");
    require(library.entries().front().saves.size()==2,"previous save became inaccessible");
    fs::path selected;
    require(library.select_save(first.id,old_save.filename().string(),selected,error) && selected==old_save,"previous save could not be selected");
    require(library.select_save(first.id,new_save.filename().string(),selected,error) && selected==new_save,"new save could not be selected");
    require(library.entries().front().active_save==new_save.filename().string(),"active save selection omitted from entry");
    fs::path reopened;
    require(library.open_world(first.id,reopened,error) && reopened==new_save,"continue did not reopen current save");
    fs::path other;
    require(library.open_world(library.entries()[1].id,other,error) && other.parent_path()!=old_save.parent_path(),"world saves share identity");
    require(WorldLibrary::resolve(other,manifest,error) && std::isfinite(manifest.spawn_y) &&
        std::abs(manifest.spawn_y-1.62f)<.1f,"raw generated spawn is not on floor");
    fs::path external;
    require(library.open_world(library.entries().back().id,external,error) && WorldLibrary::resolve(external,manifest,error),error);
    require(manifest.glb.extension()==".gltf","external glTF resolved incorrectly");
    WorldLibrary reloaded(root/"state",root/"empty-bundle");
    require(reloaded.refresh(error) && reloaded.entries().front().id==first.id,"world ID changed after restart");
    require(reloaded.open_world(first.id,reopened,error) && reopened==new_save,"save association changed after restart");
    fs::rename(sources/"0"/"main.glb",sources/"0"/"relocated.glb");
    require(reloaded.refresh(error) && !reloaded.entries().front().available,"missing world remains available");
    world_library_reset_io_stats();
    require(reloaded.locate(first.id,sources/"0"/"relocated.glb",error),error);
    const auto located_io=world_library_io_stats();
    require(!located_io.source_parses && !located_io.resource_hashes && !located_io.model_loads &&
        !located_io.prepared_catalog_reads,"Locate inspected unselected source content");
    require(!WorldLibrary::resolve(old_save,manifest,error),"unvalidated relocated source resolved directly");
    require(reloaded.open_world(first.id,reopened,error) && reopened==new_save,"selected relocated world lost active save");
    require(WorldLibrary::resolve(old_save,manifest,error) && manifest.glb.filename()=="relocated.glb","locate did not reconnect earlier saves");
    require(contents(sources/"0"/"relocated.glb")==before,"source asset changed during import");
    const auto count=reloaded.entries().size();
    for(const auto& path:{sources/"broken.glb",sources/"vertical.glb",sources/"blocked.glb",sources/"missing"/"scene.gltf"}) {
      require(reloaded.add_source(path,error),"lazy source registration inspected geometry");
      const auto invalid=reloaded.entries().back();
      require(!reloaded.open_world(invalid.id,selected,error) && !error.empty(),"invalid selected source opened");
      require(!fs::exists(root/"state"/"saves"/"worlds"/invalid.id),"invalid selected source created a save");
    }
    require(reloaded.entries().size()==count+4,"lazy invalid-source identities were lost");
    const auto marker=old_save/"world.json";const auto original=contents(marker);
    write(marker,"{\"version\":1,\"world_id\":\"../escape\",\"save_id\":\"escape\"}");
    require(!WorldLibrary::resolve(old_save,manifest,error) && !error.empty(),"invalid save identity accepted");
    write(marker,original);
    require(contents(old_save/"client"/"inventory.json")=="existing-inventory" &&
        contents(new_save/"client"/"inventory.json")=="new-inventory","save contents crossed world boundaries");
    const auto texture=sources/"external"/"texture image.png";
    fs::last_write_time(texture,fs::last_write_time(texture)+std::chrono::seconds(1));
    require(WorldLibrary::resolve(external,manifest,error),"identical resource with new timestamp rejected");
    const auto texture_before=contents(texture);
    auto texture_changed=texture_before;texture_changed.back()=char(texture_changed.back()^1);write(texture,texture_changed);
    require(!WorldLibrary::resolve(external,manifest,error),"changed texture silently reused prior save");
    write(texture,texture_before);
    require(WorldLibrary::resolve(external,manifest,error),"restored source resource rejected");
    const auto unicode=sources/fs::path(u8"世界.glb");
    require(reloaded.add_source(unicode,error),error);
    const auto authored=sources/"authored"/"main.glb";
    require(reloaded.add_source(authored,error),error);
    fs::path authored_save;
    require(reloaded.open_world(reloaded.entries().back().id,authored_save,error) && WorldLibrary::resolve(authored_save,manifest,error),error);
    require(manifest.spawn_x==1 && manifest.spawn_y==3 && manifest.spawn_z==-9,"authored spawn replaced");
    const auto tiled_source=sources/"tiled"/"main.glb";
    require(reloaded.add_source(tiled_source,error),error);
    fs::path tiled_save;
    require(reloaded.open_world(reloaded.entries().back().id,tiled_save,error) && WorldLibrary::resolve(tiled_save,manifest,error),error);
    require(manifest.tiled && manifest.spawn_x==49,"authored tiled spawn did not use nearby tile geometry");
    const auto far_tile=sources/"tiled"/"tiles"/"tile.glb";const auto tile_original=contents(far_tile);
    auto changed_tile=tile_original;changed_tile.back()=char(changed_tile.back()^1);write(far_tile,changed_tile);
    require(!WorldLibrary::resolve(tiled_save,manifest,error),"changed authored tile silently reused prior save");
    write(far_tile,tile_original);
    require(WorldLibrary::resolve(tiled_save,manifest,error),"restored authored tile rejected");
    const auto live_catalog=root/"state"/"saves"/"worlds"/"catalog.json";
    WorldLibraryCatalog incomplete;require(world_library_load_catalog(live_catalog,incomplete,error),error);
    const auto tiled_id=reloaded.entries().back().id;
    for(auto& world:incomplete.worlds)if(world.id==tiled_id)
      std::erase_if(world.resources,[](const auto& resource){return fs::path(reinterpret_cast<const char8_t*>(resource.path.c_str())).filename()=="tile.glb";});
    std::string incomplete_json;require(!glz::write_json(incomplete,incomplete_json),"catalog fixture serialization failed");write(live_catalog,incomplete_json);
    WorldLibrary upgraded(root/"state",root/"empty-bundle");require(upgraded.refresh(error),error);
    require(upgraded.entries().back().id==tiled_id && upgraded.entries().back().active_save==tiled_save.filename().string(),"resource upgrade changed world or save identity");
    require(upgraded.open_world(tiled_id,reopened,error) && reopened==tiled_save,"selected source resource coverage was not upgraded");
    write(far_tile,changed_tile);require(!WorldLibrary::resolve(tiled_save,manifest,error),"older catalog resource coverage was not expanded");
    write(far_tile,tile_original);require(WorldLibrary::resolve(tiled_save,manifest,error),"upgraded tile fingerprint rejected restored contents");
    WorldLibrary discovered(root/"discovered",root/"empty-bundle");
    require(discovered.scan_folder(sources/"tiled",error),error);
    require(discovered.entries().size()==1,"authored tile payloads appeared as separate worlds");
    std::atomic_bool cancel{true};
    require(!reloaded.scan_folder(sources,error,&cancel),"canceled folder scan continued");
    require(!reloaded.locate(first.id,sources/"0"/"relocated.glb",error,&cancel),"canceled locate continued");
    const auto catalog=root/"state"/"saves"/"worlds"/"catalog.json";const auto original_catalog=contents(catalog);
    auto capped_catalog=original_catalog;const auto count_offset=capped_catalog.find("\"save_count\":2");
    require(count_offset!=std::string::npos,"save count fixture missing");
    capped_catalog.replace(count_offset,std::string("\"save_count\":2").size(),"\"save_count\":100000");write(catalog,capped_catalog);
    WorldLibrary capped(root/"state",root/"empty-bundle");require(capped.refresh(error),error);
    require(!capped.new_save(first.id,selected,error),"maximum save count exceeded");write(catalog,original_catalog);
    std::printf("world_library_probe=passed worlds=13 valid_worlds=9 deferred_invalid=4 stable_identity=1 isolated_saves=1 selectable_saves=1 external_gltf=1 unicode_paths=1 authored_spawn=1 tiled_spawn=1 tile_scan_grouping=1 source_unchanged=1 resource_fingerprint=1 collision_spawn=1 locate_cancel=1 save_limit=1 authored_migration=1 raw_neighbor_isolation=1 legacy_identity=1\n");
    return 0;
  } catch(const std::exception& failure) {std::fprintf(stderr,"world_library_probe=failed reason=%s\n",failure.what());return 1;}
}
