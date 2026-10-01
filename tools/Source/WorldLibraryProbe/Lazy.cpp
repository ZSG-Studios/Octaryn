#include "WorldLibrary.h"
#include "WorldLibraryRecords.h"
#include "WorldLibraryIo.h"
#include "MapManifest.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {
namespace fs=std::filesystem;
using namespace octaryn::client::app;
void require(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
std::string read(const fs::path& path) {std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
void write(const fs::path& path,const std::string& text) {
  fs::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary);file<<text;
  require(bool(file),"lazy probe could not write fixture marker");
}
bool zero() {
  const auto io=world_library_io_stats();
  return !io.source_parses && !io.resource_hashes && !io.model_loads && !io.prepared_catalog_reads;
}
void report(const char* phase) {
  const auto io=world_library_io_stats();
  std::printf("world_library_io phase=%s source_parses=%llu resource_hashes=%llu model_loads=%llu prepared_catalog_reads=%llu\n",
      phase,static_cast<unsigned long long>(io.source_parses),static_cast<unsigned long long>(io.resource_hashes),
      static_cast<unsigned long long>(io.model_loads),static_cast<unsigned long long>(io.prepared_catalog_reads));
}
const WorldLibraryEntry& entry(const WorldLibrary& library,const fs::path& source) {
  const auto found=std::find_if(library.entries().begin(),library.entries().end(),[&](const auto& world) {
    return fs::path(reinterpret_cast<const char8_t*>(world.source.c_str())).lexically_normal()==source.lexically_normal();
  });
  require(found!=library.entries().end(),"lazy source was not listed");return *found;
}
class PayloadLocks {
#ifdef _WIN32
  std::map<fs::path,HANDLE> handles;
#endif
public:
  explicit PayloadLocks(const std::vector<fs::path>& paths) {
#ifdef _WIN32
    for(const auto& path:paths) {
      const auto handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_WRITE|FILE_SHARE_DELETE,
          nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
      if(handle==INVALID_HANDLE_VALUE) {
        for(const auto& [prior,opened]:handles)CloseHandle(opened);
        handles.clear();throw std::runtime_error("could not deny payload reads for the lazy probe");
      }
      handles.emplace(path,handle);
    }
#else
    (void)paths;
#endif
  }
  ~PayloadLocks() {
#ifdef _WIN32
    for(const auto& [path,handle]:handles)CloseHandle(handle);
#endif
  }
  void release(const fs::path& path) {
#ifdef _WIN32
    if(const auto found=handles.find(path);found!=handles.end()) {CloseHandle(found->second);handles.erase(found);}
#else
    (void)path;
#endif
  }
};
}

void test_world_library_registration(const std::filesystem::path& source,const std::filesystem::path& root) {
  using namespace octaryn::client::app;
  std::vector<fs::path> payloads;
  for(const auto& item:fs::directory_iterator(source.parent_path()))if(item.is_regular_file()) {
    const auto extension=item.path().extension().string();
    if(extension==".glb" || extension==".gltf" || extension==".bin")payloads.push_back(item.path());
  }
  require(std::find(payloads.begin(),payloads.end(),source)!=payloads.end(),"registration source missing");
  PayloadLocks locks(payloads);world_library_reset_io_stats();std::string error;
  WorldLibrary library(root,root/"empty-bundle");
  require(library.refresh(error) && library.add_source(source,error) && library.refresh(error),error);
  require(library.entries().size()==1 && zero(),"registration read source payloads");
  const auto id=entry(library,source).id;
  WorldLibrary restarted(root,root/"empty-bundle");require(restarted.refresh(error),error);
  require(restarted.entries().size()==1 && entry(restarted,source).id==id && zero(),
      "registration restart changed identity or read payloads");
  require(entry(restarted,source).saves.empty() && !fs::exists(root/"saves"/"worlds"/id) &&
      !fs::exists(root/"geometry-cache"),"registration created a save or geometry cache");
  report("unselected");
  std::printf("world_library_registration=passed locked_payloads=%zu restart_identity=1 saves_created=0 source_validation_deferred=1\n",payloads.size());
}

void test_world_library_lazy(const std::filesystem::path& fixture) {
  using namespace octaryn::client::app;
  const auto root=fixture/"automatic",bundle=root/"empty-bundle",maps=root/"octaryn-client"/"Assets"/"Maps";
  const auto first=maps/"main.glb",second=maps/"second"/"main.glb",broken=maps/"broken"/"broken.glb";
  const auto external=maps/"external"/"scene.gltf",directory=root/"saves"/"worlds";
  std::vector<fs::path> paths;std::map<fs::path,std::string> original;
  for(const auto& item:fs::recursive_directory_iterator(maps))if(item.is_regular_file()) {
    const auto extension=item.path().extension().string();
    if(extension==".glb" || extension==".gltf" || extension==".bin" || extension==".png") {
      paths.push_back(item.path());original.emplace(item.path(),read(item.path()));
    }
  }
  require(paths.size()>=6,"lazy fixture resources missing");PayloadLocks locks(paths);
#ifdef _WIN32
  require(!std::ifstream(first,std::ios::binary),"payload deny-read lock was ineffective");
#endif
  world_library_reset_io_stats();std::string error;fs::path selected;
  WorldLibrary library(root,bundle);require(library.refresh(error),error);
  require(library.entries().size()==4 && zero(),"menu discovery inspected source content");
  const auto first_id=entry(library,first).id,second_id=entry(library,second).id;
  const auto broken_id=entry(library,broken).id,external_id=entry(library,external).id;
  require(library.add_source(first,error) && library.scan_folder(maps,error),error);
  require(library.entries().size()==4 && zero(),"Add/Find inspected an unselected source");
  WorldLibrary restarted(root,bundle);require(restarted.refresh(error),error);
  require(entry(restarted,first).id==first_id && entry(restarted,second).id==second_id && zero(),
      "restart changed identity or inspected source content");
  for(const auto& world:restarted.entries())require(world.saves.empty() && !fs::exists(directory/world.id),
      "unselected source created a save");
  require(!fs::exists(root/"geometry-cache"),"menu created a geometry cache");report("unselected");

  std::atomic_bool cancel{true};
  require(!restarted.open_world(first_id,selected,error,&cancel) && !error.empty() && zero(),
      "pre-canceled selection performed content work");
  require(!fs::exists(directory/first_id),"pre-canceled selection created a save");
  locks.release(first);cancel=false;unsigned progress{};
  require(!restarted.open_world(first_id,selected,error,&cancel,[&](const std::string& text) {
    require(!text.empty(),"loading progress omitted its action");++progress;cancel=true;
  }) && progress && !error.empty(),"selection ignored progress-driven cancellation");
  require(!fs::exists(directory/first_id),"canceled selection published a save");
  cancel=false;progress=0;world_library_reset_io_stats();
  require(restarted.open_world(first_id,selected,error,&cancel,[&](const std::string& text) {
    require(!text.empty(),"selected loading progress is empty");++progress;
  }),error);
  const auto inspected=world_library_io_stats();
  require(progress && inspected.source_parses && inspected.resource_hashes && inspected.model_loads,
      "selected world did not execute source, integrity and collision validation");report("selected");
  const auto first_save=selected;
  write(first_save/"client"/"inventory.json","first-progress");
  world_library_reset_io_stats();
  fs::last_write_time(first,fs::last_write_time(first)+std::chrono::seconds(1));
  require(restarted.refresh(error) && zero(),"menu refresh rehashed a timestamp-changed selected source");
  WorldLibrary listed(root,bundle);require(listed.refresh(error) && zero(),"saved-world listing inspected source content");
  require(entry(listed,first).saves.size()==1 && entry(listed,first).active_save==first_save.filename().string(),
      "lazy refresh lost the active save");report("saved_menu");
  MapManifest manifest;
  require(WorldLibrary::resolve(first_save,manifest,error) && fs::equivalent(manifest.glb,first),error);
  require(listed.new_save(first_id,selected,error),error);const auto other_first_save=selected;
  require(first_save!=other_first_save && !fs::exists(other_first_save/"client"/"inventory.json"),"New Save shared progress");
  locks.release(second);
  require(listed.open_world(second_id,selected,error),error);const auto second_save=selected;
  require(second_save.parent_path()!=first_save.parent_path() && !fs::exists(second_save/"client"/"inventory.json"),
      "independent worlds shared a save");
  require(listed.select_save(first_id,first_save.filename().string(),selected,error) && selected==first_save &&
      read(first_save/"client"/"inventory.json")=="first-progress","save selection lost prior progress");
  cancel=true;world_library_reset_io_stats();
  require(!listed.new_save(first_id,selected,error,&cancel) &&
      !listed.select_save(first_id,other_first_save.filename().string(),selected,error,&cancel) && zero(),
      "canceled save action performed content work");
  require(entry(listed,first).saves.size()==2 && entry(listed,first).active_save==first_save.filename().string(),
      "canceled save action changed selected progress");
  cancel=false;
  locks.release(broken);
  require(!listed.open_world(broken_id,selected,error) && !error.empty() && !fs::exists(directory/broken_id),
      "invalid selected source failed silently or created a save");
  for(const auto& path:paths)locks.release(path);
  require(listed.open_world(external_id,selected,error) && WorldLibrary::resolve(selected,manifest,error) &&
      fs::equivalent(manifest.glb,external),error);
  for(const auto& [path,bytes]:original)require(read(path)==bytes,"lazy library modified source content");
  std::puts("world_library_lazy=passed unselected_reads=0 selected_validation=1 progress=1 cancel=1 invalid_selection=1 isolated_saves=1 external_gltf=1 source_unchanged=1");
}
