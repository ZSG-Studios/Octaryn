#include "Encode.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace octaryn::client::rendering;
namespace {
struct Variant {size_t image{};MapMipOptions options;std::string key;};
void cook(const std::filesystem::path& source,const std::filesystem::path& output,bool bc7,unsigned jobs) {
  MapModel model;std::string error;
  if(!load_map_texture_catalog(source,model,error))throw std::runtime_error(error);
  const auto source_hash=map_texture_file_digest(source,error);
  if(source_hash.empty())throw std::runtime_error(error);
  std::map<std::pair<size_t,MapMipOptions>,bool> requests;
  for(const auto& primitive:model.primitives)for(unsigned role=0;role<5;++role) {
    const auto image=primitive.material.textures[role].image;
    if(image<0)continue;
    const auto options=map_mip_options(primitive.material,role);
    requests.emplace(std::make_pair(size_t(image),options),true);
  }
  std::map<std::string,Variant> unique;
  for(const auto& [request,unused]:requests) {
    const auto key=map_texture_cache_key(model.images.at(request.first),request.second);
    unique.emplace(key,Variant{request.first,request.second,key});
  }
  std::filesystem::create_directories(output);
  std::vector<Variant> variants;for(const auto& [key,variant]:unique)variants.push_back(variant);
  std::atomic<size_t> next{},completed{};std::mutex failure_mutex;std::exception_ptr failure;
  const auto worker=[&] {
    try {
      while(true) {
        const auto index=next.fetch_add(1);if(index>=variants.size())break;
        const auto& variant=variants[index];
        const bool color=variant.options.role==MapMipRole::BaseColor || variant.options.role==MapMipRole::Emissive;
        const auto path=output/(variant.key+".dds");
        MapCachedTexture cached;std::string reason;
        // Alpha-bearing and data/normal images retain exact mip values.
        const bool compress=bc7 && color;
        if(read_map_texture_cache(path,0,0,color,cached,reason)==MapCacheResult::Ready) {
          const bool eligible=cached.opaque && cached.levels.front().width%4==0 && cached.levels.front().height%4==0;
          if(cached.compressed==(compress && eligible)) {++completed;continue;}
        }
        MapDecodedImage image;
        if(!decode_map_image(model.images[variant.image],image,reason))throw std::runtime_error(reason);
        auto levels=build_map_mips(image,variant.options);
        bool opaque=true;for(size_t pixel=3;pixel<image.rgba.size();pixel+=4)if(image.rgba[pixel]!=255) {opaque=false;break;}
        // Non-multiple-of-four dimensions remain lossless for portable GPU uploads.
        auto texture=compress && opaque && image.width%4==0 && image.height%4==0?
            encode_map_bc7(levels,color):lossless_map_texture_cache(levels,color);
        texture.opaque=opaque;
        if(!write_map_texture_cache(path,texture,reason))throw std::runtime_error(reason);
        const auto count=++completed;
        if(count%16==0)std::printf("map_texture_cook progress=%zu/%zu\n",size_t(count),variants.size());
      }
    } catch(...) {std::lock_guard lock(failure_mutex);if(!failure)failure=std::current_exception();}
  };
  std::vector<std::thread> workers;
  for(unsigned i=0;i<std::min<unsigned>(jobs,std::max<size_t>(1,variants.size()));++i)workers.emplace_back(worker);
  for(auto& thread:workers)thread.join();if(failure)std::rethrow_exception(failure);
  const auto temporary=output/"map-texture-cook.json.tmp";
  std::ofstream manifest(temporary,std::ios::trunc);
  manifest<<"{\"version\":"<<map_texture_cache_version<<",\"status\":\"complete\",\"codec\":\""
      <<(bc7?"bc7-opaque-color-uber4":"rgba8-lossless")<<"\",\"map_sha256\":\""<<source_hash<<"\",\"files\":[";
  for(size_t i=0;i<variants.size();++i)manifest<<(i?",":"")<<'"'<<variants[i].key<<'"';
  manifest<<"]}\n";manifest.close();if(!manifest)throw std::runtime_error("manifest write failed");
  std::filesystem::rename(temporary,output/"map-texture-cook.json");
  std::printf("map_texture_cook variants=%zu complete=1 mode=%s\n",variants.size(),bc7?"bc7-color":"lossless");
}
}
int main(int argc,char** argv) {
  try {
    if(argc==3 && std::string(argv[1])=="--self-test")return test_map_texture_cook(argv[2])?0:1;
    if(argc==5 && std::string(argv[1])=="--compare")return compare_map_texture_caches(argv[2],argv[3],argv[4])?0:1;
    if(argc<3)throw std::runtime_error("usage: map_texture_cook map.glb output-directory [--bc7] [--jobs N]");
    bool bc7=false;unsigned jobs=std::clamp(std::thread::hardware_concurrency()/2,1u,4u);
    for(int i=3;i<argc;++i) {
      if(std::string(argv[i])=="--bc7")bc7=true;
      else if(std::string(argv[i])=="--jobs" && i+1<argc)jobs=std::clamp(unsigned(std::stoul(argv[++i])),1u,8u);
      else throw std::runtime_error("unknown cooker option");
    }
    cook(argv[1],argv[2],bc7,jobs);return 0;
  } catch(const std::exception& error) {std::fprintf(stderr,"map_texture_cook_failed: %s\n",error.what());return 1;}
}
