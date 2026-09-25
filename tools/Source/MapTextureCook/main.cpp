#include "Encode.h"
#include <cstdio>
#include <map>
#include <set>
#include <charconv>
#include <fstream>
#include <chrono>

using namespace map_texture_cook;
int main(int argc,char** argv) {
  if(argc==3 && std::string(argv[1])=="--self-test")return self_test(argv[2])?0:1;
  const auto start=std::chrono::steady_clock::now();
  unsigned maximum=~0u;
  if(argc<2) {std::fprintf(stderr,"usage: octaryn_map_texture_cook MAP.glb [--output-dir DIR] [--max-textures N] [--reuse-cache DIR]\n");return 2;}
  const std::filesystem::path path(reinterpret_cast<const char8_t*>(argv[1]));
  auto directory=path;directory+=".textures";std::filesystem::path reuse;bool pilot=false;
  for(int arg=2;arg<argc;arg+=2) {
    if(arg+1>=argc)return 2;
    const std::string option=argv[arg];
    if(option=="--output-dir")directory=std::filesystem::path(reinterpret_cast<const char8_t*>(argv[arg+1]));
    else if(option=="--reuse-cache")reuse=std::filesystem::path(reinterpret_cast<const char8_t*>(argv[arg+1]));
    else if(option=="--max-textures") {
      const auto result=std::from_chars(argv[arg+1],argv[arg+1]+std::char_traits<char>::length(argv[arg+1]),maximum);
      if(result.ec!=std::errc{} || *result.ptr!='\0' || !maximum)return 2;pilot=true;
    } else return 2;
  }
  MapModel model;std::string error;
  const auto original_hash=map_texture_file_digest(path,error);if(original_hash.empty()) {std::fprintf(stderr,"source hash: %s\n",error.c_str());return 1;}
  if(!load_map_texture_catalog(path,model,error)) {std::fprintf(stderr,"map texture catalog: %s\n",error.c_str());return 1;}
  std::error_code ec;std::filesystem::create_directories(directory,ec);
  if(ec) {std::fprintf(stderr,"cache directory: %s\n",ec.message().c_str());return 1;}
  std::map<size_t,std::set<MapMipOptions>> variants;
  for(const auto& primitive:model.primitives)for(unsigned slot=0;slot<5;++slot) {
    const auto image=primitive.material.textures[slot].image;
    if(image>=0)variants[static_cast<size_t>(image)].insert(map_mip_options(primitive.material,slot));
  }
  unsigned cooked{},existing{},rejected{},lossless{},reused{};std::set<std::string> files;
  for(const auto& [image,options_set]:variants) {
    MapDecodedImage pixels;
    if(!decode_map_image(model.images[image],pixels,error)) {std::fprintf(stderr,"decode: %s\n",error.c_str());return 1;}
    for(const auto& options:options_set) {
      if(cooked+existing>=maximum)break;
      const auto key=map_texture_cache_key(model.images[image],options);
      const auto target=directory/(key+".dds");
      MapCachedTexture encoded;Quality quality;
      const bool srgb=options.role==MapMipRole::BaseColor || options.role==MapMipRole::Emissive;
      const auto cached=read_map_texture_cache(target,pixels.width,pixels.height,srgb,encoded,error);
      if(cached==MapCacheResult::Ready) {++existing;files.insert(key);continue;}
      if(cached==MapCacheResult::Invalid) {std::fprintf(stderr,"refusing to replace invalid existing cache %s: %s\n",key.c_str(),error.c_str());return 1;}
      if(!reuse.empty() && read_map_texture_cache(reuse/(key+".dds"),pixels.width,pixels.height,srgb,encoded,error)==MapCacheResult::Ready) {
        if(!write_map_texture_cache(target,encoded,error)) {std::fprintf(stderr,"cache reuse: %s\n",error.c_str());return 1;}
        ++cooked;++reused;files.insert(key);continue;
      }
      const auto mips=build_map_mips(pixels,options);
      if(!encode(mips,options,encoded,quality,error)) {
        ++rejected;++lossless;std::printf("texture_bc7_rejected image=%zu role=%u reason=%s lossless_cache=1\n",image,unsigned(options.role),error.c_str());
        encoded=lossless_map_texture_cache(mips,srgb);
      }
      if(!write_map_texture_cache(target,encoded,error)) {std::fprintf(stderr,"cache write: %s\n",error.c_str());return 1;}
      ++cooked;files.insert(key);std::printf("texture_cooked image=%zu role=%u bc7=%u mips=%zu max_error=%u normal_max=%.3f mask_changed=%u\n",
          image,unsigned(options.role),encoded.compressed?1:0,mips.size(),quality.maximum_error,quality.maximum_normal_degrees,quality.mask_changed);
      std::fflush(stdout);
    }
    if(cooked+existing>=maximum)break;
  }
  const auto hash=map_texture_file_digest(path,error);if(hash.empty() || hash!=original_hash) {std::fprintf(stderr,"source changed during cook or unreadable: %s\n",error.c_str());return 1;}
  auto manifest=directory/"map-texture-cook.json",temporary=directory/"map-texture-cook.json.tmp";
  std::ofstream report(temporary,std::ios::binary|std::ios::trunc);
  report<<"{\"version\":"<<map_texture_cache_version<<",\"status\":\""<<(pilot?"pilot":"complete")<<"\",\"map_sha256\":\""<<hash<<"\",\"files\":[";
  bool first=true;for(const auto& key:files) {if(!first)report<<',';report<<'"'<<key<<'"';first=false;}report<<"]}\n";
  report.close();if(!report)return 1;
  std::filesystem::remove(manifest,ec);if(ec)return 1;std::filesystem::rename(temporary,manifest,ec);if(ec)return 1;
  const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  std::printf("map_texture_cook cooked=%u existing=%u reused=%u quality_rejected=%u lossless=%u status=%s seconds=%.3f runtime_encoding=0\n",
      cooked,existing,reused,rejected,lossless,pilot?"pilot":"complete",elapsed);
  return 0;
}
