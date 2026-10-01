#include "SpatialTriangleOrder.h"
#include "FilePath.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace octaryn::scene_geometry;
namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
SpatialTriangle triangle(std::uint64_t index) {
  const auto x=float(index%16)*100;
  return {{x+.25f,.25f,0},{x,0,0,x+1,1,0}};
}
bool source(std::uint64_t first,std::span<SpatialTriangle> output,std::string&) {
  for(std::size_t i=0;i<output.size();++i)output[i]=triangle(first+i);return true;
}
}
int main(int argc,char** argv) {
  try {
    require(argc==2,"usage: scene_spatial_order_probe output-directory");
    const auto root=octaryn::content::file_io_path(std::filesystem::path(reinterpret_cast<const char8_t*>(argv[1])));std::filesystem::create_directories(root);
    const auto output=root/"source.triangles";SpatialOrderConfig config;
    config.source_hash=std::string(64,'a');config.mesh=3;config.primitive=2;config.triangles=131072;
    config.run_triangles=4096;config.part_triangles=512;config.merge_width=4;config.maximum_scratch_bytes=16ull<<20;
    config.bounds={0,0,0,1501,1,0};SpatialOrderResult result;std::string error;
    const auto success=[&](bool value){if(!value)throw std::runtime_error(error);};
    require(!std::filesystem::exists(output),"use a fresh output directory for the spatial-order probe");
    success(write_spatial_triangle_order(output,config,source,result,error));
    std::filesystem::path confined;
    success(spatial_triangle_order_path(root/"scene.json","source.triangles",confined,error));
    require(confined==octaryn::content::canonical_file_path(output),"spatial order confinement changed output identity");
    require(!spatial_triangle_order_path(root/"scene.json","../source.triangles",confined,error),"spatial order accepted escaping path");
    success(validate_spatial_triangle_order(output,config,error));
    require(result.triangles==config.triangles && result.parts.size()==256 && result.merge_passes==3 &&
        result.peak_records<=config.run_triangles && result.peak_scratch_bytes<=config.maximum_scratch_bytes,"spatial order exceeded its bounds");
    std::vector<bool> seen(config.triangles);std::uint64_t total{};
    for(const auto& part:result.parts) {
      require(part.triangle_count==512 && part.bounds[3]-part.bounds[0]==1,"spatial partition kept distant interleaved triangles together");
      std::vector<std::uint64_t> ids;success(read_spatial_triangle_order(output,config,part.first_triangle,std::uint32_t(part.triangle_count),ids,error));
      for(auto id:ids) {
        require(!seen[id],"spatial order duplicated a source triangle");seen[id]=true;++total;
        const auto original=triangle(id);
        for(unsigned axis=0;axis<3;++axis)require(original.bounds[axis]>=part.bounds[axis] &&
            original.bounds[axis+3]<=part.bounds[axis+3],"spatial part bounds missed source geometry");
      }
    }
    require(total==config.triangles && std::all_of(seen.begin(),seen.end(),[](bool value){return value;}),"spatial order lost source coverage");
    const auto stamp=std::filesystem::last_write_time(output);SpatialOrderResult rejected;
    require(!write_spatial_triangle_order(output,config,source,rejected,error) && std::filesystem::last_write_time(output)==stamp,"spatial order overwrote an existing asset");
    auto changed=config;changed.source_hash[0]='b';std::vector<std::uint64_t> ids;
    require(!read_spatial_triangle_order(output,changed,0,1,ids,error),"spatial order accepted changed source content");
    changed=config;++changed.primitive;require(!read_spatial_triangle_order(output,changed,0,1,ids,error),"spatial order accepted wrong material primitive");
    require(!read_spatial_triangle_order(output,config,config.triangles,1,ids,error),"spatial order accepted out-of-bounds range");
    const auto duplicate=root/"duplicate.triangles";std::filesystem::copy_file(output,duplicate);
    success(read_spatial_triangle_order(output,config,0,1,ids,error));
    {std::fstream file(duplicate,std::ios::binary|std::ios::in|std::ios::out);file.seekp(96+8);file.write(reinterpret_cast<const char*>(ids.data()),8);}
    require(!validate_spatial_triangle_order(duplicate,config,error),"spatial order accepted duplicated/missing source triangles");
    std::filesystem::remove(duplicate);
    std::atomic_bool cancel{};const auto canceled=root/"canceled.triangles";
    const auto stop=[&](auto first,auto records,auto& message){source(first,records,message);cancel=true;return true;};
    require(!write_spatial_triangle_order(canceled,config,stop,rejected,error,&cancel) && !std::filesystem::exists(canceled),"canceled spatial order published partial geometry");
    changed=config;changed.maximum_scratch_bytes=128;const auto limited=root/"limited.triangles";
    require(!write_spatial_triangle_order(limited,changed,source,rejected,error) && !std::filesystem::exists(limited),"scratch budget published partial geometry");
    unsigned files{};for(const auto& entry:std::filesystem::directory_iterator(root)) {++files;require(entry.path()==output,"spatial order leaked scratch files");}
    require(files==1,"spatial order output missing");
    std::printf("scene_spatial_order passed=1 triangles=%llu parts=%zu runs_peak=%llu scratch_peak=%llu merge_passes=%u complete_permutation=1 source_identity=1 exact_bounds=1 cancel=1 budget=1 source_order_width=1501 spatial_width=1\n",
        static_cast<unsigned long long>(result.triangles),result.parts.size(),static_cast<unsigned long long>(result.peak_records),
        static_cast<unsigned long long>(result.peak_scratch_bytes),result.merge_passes);return 0;
  }catch(const std::exception& failure) {std::fprintf(stderr,"scene_spatial_order failed=%s\n",failure.what());return 1;}
}
