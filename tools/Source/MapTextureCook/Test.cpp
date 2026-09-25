#include "Encode.h"
#include "../ClientWorldMeshProbe/MapMipFixture.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>

namespace map_texture_cook {
bool self_test(const std::filesystem::path& parent) {
  unsigned checks{},failed{};
  const auto check=[&](bool value,const char* name) {++checks;if(!value) {++failed;std::fprintf(stderr,"texture_cook_check failed: %s\n",name);}};
  const auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
  const auto root=parent/("run-"+std::to_string(nonce));
  std::error_code ec;std::filesystem::create_directories(root,ec);if(ec)return false;
  check(map_texture_digest({})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA256 empty");
  const std::vector<std::uint8_t> abc{'a','b','c'};
  check(map_texture_digest(abc)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 abc");
  const std::vector<std::uint8_t> repeated(1000000,'a');
  check(map_texture_digest(repeated)=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","SHA256 million a");
  const char* padding_hashes[]={"9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318",
      "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a",
      "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"};
  unsigned padding_index{};for(unsigned length:{55u,56u,64u})
    check(map_texture_digest(std::vector<std::uint8_t>(length,'a'))==padding_hashes[padding_index++],"SHA256 padding vs Python hashlib");
  MapModelImage source;source.bytes=abc;MapMipOptions options;
  const auto key=map_texture_cache_key(source,options);
  auto changed=options;changed.alpha_factor=.25f;check(key!=map_texture_cache_key(source,changed),"factor in key");
  changed=options;changed.alpha_cutoff=.25f;check(key!=map_texture_cache_key(source,changed),"cutoff in key");
  changed=options;changed.alpha_weighted=true;check(key!=map_texture_cache_key(source,changed),"alpha weighting in key");
  changed=options;changed.preserve_coverage=true;check(key!=map_texture_cache_key(source,changed),"coverage in key");
  changed=options;changed.role=MapMipRole::Normal;check(key!=map_texture_cache_key(source,changed),"role in key");
  source.bytes.push_back(0);check(key!=map_texture_cache_key(source,options),"encoded content in key");
  MapDecodedImage image{8,8,{}};image.rgba.resize(8*8*4);
  for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
    auto* pixel=&image.rgba[(y*8+x)*4];pixel[0]=static_cast<std::uint8_t>(32+x*8);
    pixel[1]=static_cast<std::uint8_t>(64+y*8);pixel[2]=80;pixel[3]=255;
  }
  MapCachedTexture encoded,loaded;Quality quality;std::string error;
  check(encode(build_map_mips(image,options),options,encoded,quality,error),"color BC7 quality gate");
  check(quality.maximum_error<=24,"color max channel error");
  const auto file=root/"color.dds";
  check(write_map_texture_cache(file,encoded,error),"write standard DX10 DDS");
  check(read_map_texture_cache(file,8,8,true,loaded,error)==MapCacheResult::Ready,"read full chain");
  check(map_texture_file_digest(file,error).size()==64,"streaming file hash");
  check(loaded.levels.size()==4 && loaded.levels.front().blocks==encoded.levels.front().blocks,"cache payload roundtrip");
  check(read_map_texture_cache(file,8,8,false,loaded,error)==MapCacheResult::Invalid,"wrong color space rejected");
  check(read_map_texture_cache(file,16,8,true,loaded,error)==MapCacheResult::Invalid,"wrong dimensions rejected");
  check(read_map_texture_cache(root/"missing.dds",8,8,true,loaded,error)==MapCacheResult::Missing,"missing distinct from corrupt");
  check(read_map_texture_cache(file,8192,8,true,loaded,error)==MapCacheResult::Invalid,"bounded dimension rejection");
  {std::ofstream append(file,std::ios::binary|std::ios::app);append.put(0);}
  check(read_map_texture_cache(file,8,8,true,loaded,error)==MapCacheResult::Invalid,"trailing payload rejected");
  const auto bad=root/"bad.dds";check(write_map_texture_cache(bad,encoded,error),"second DDS write");
  {std::fstream edit(bad,std::ios::binary|std::ios::in|std::ios::out);edit.seekp(148);edit.put(0);}
  check(read_map_texture_cache(bad,8,8,true,loaded,error)==MapCacheResult::Invalid,"invalid BC7 mode rejected");
  const auto corrupt=root/"corrupt.dds";check(write_map_texture_cache(corrupt,encoded,error),"integrity DDS write");
  {std::fstream edit(corrupt,std::ios::binary|std::ios::in|std::ios::out);edit.seekp(151);edit.put(17);}
  check(read_map_texture_cache(corrupt,8,8,true,loaded,error)==MapCacheResult::Invalid,"valid-mode payload corruption rejected");
  const auto no_hash=root/"no-hash.dds";std::filesystem::copy_file(corrupt,no_hash);
  check(read_map_texture_cache(no_hash,8,8,true,loaded,error)==MapCacheResult::Invalid,"missing integrity companion rejected");
  const auto truncated=root/"truncated.dds";{std::ofstream file_out(truncated,std::ios::binary);file_out<<"DDS ";}
  check(read_map_texture_cache(truncated,8,8,true,loaded,error)==MapCacheResult::Invalid,"truncated header rejected");
  options.role=MapMipRole::Normal;
  for(size_t i=0;i<image.rgba.size();i+=4) {image.rgba[i]=128;image.rgba[i+1]=128;image.rgba[i+2]=255;image.rgba[i+3]=255;}
  check(encode(build_map_mips(image,options),options,encoded,quality,error),"flat normal BC7 gate");
  check(quality.maximum_normal_degrees<=5,"normal angular error bounded");
  options.role=MapMipRole::BaseColor;options.alpha_weighted=true;options.preserve_coverage=true;
  for(size_t i=0;i<image.rgba.size();i+=4) {image.rgba[i]=64;image.rgba[i+1]=160;image.rgba[i+2]=32;image.rgba[i+3]=(i/4)%2?0:255;}
  const bool mask_ok=encode(build_map_mips(image,options),options,encoded,quality,error);
  check(mask_ok?quality.mask_changed==0:encoded.levels.empty(),"foliage either exact MASK decisions or fail closed");
  check(mask_ok || error=="opaque alpha changed" || error=="BC7 quality gate rejected variant","foliage rejection is explicit quality gate");
  std::printf("texture_cook_mask accepted=%u reason=%s\n",mask_ok?1:0,error.c_str());
  options.alpha_factor=.5f;
  const bool factor_ok=encode(build_map_mips(image,options),options,encoded,quality,error);
  check(factor_ok?quality.mask_changed==0:encoded.levels.empty(),"factor MASK decisions preserved or cache withheld");
  check(factor_ok || !error.empty(),"factor rejection diagnosed");
  std::printf("texture_cook_mask_factor accepted=%u reason=%s\n",factor_ok?1:0,error.c_str());
  const auto mask_mips=build_map_mips(image,options);
  const auto mask_cache=lossless_map_texture_cache(mask_mips,true);
  const auto lossless_file=root/"lossless.dds";
  check(write_map_texture_cache(lossless_file,mask_cache,error),"write lossless RGBA DDS");
  check(read_map_texture_cache(lossless_file,8,8,true,loaded,error)==MapCacheResult::Ready && !loaded.compressed,"read lossless RGBA DDS");
  bool exact=loaded.levels.size()==mask_mips.size();
  for(size_t mip=0;mip<loaded.levels.size() && mip<mask_mips.size();++mip)exact&=loaded.levels[mip].blocks==mask_mips[mip].rgba;
  check(exact,"lossless cache preserves every mask mip byte");
  const MapDecodedImage odd{3,1,{255,0,0,255,0,255,0,128,0,0,255,0}};
  const auto odd_cache=lossless_map_texture_cache(build_map_mips(odd,options),false);
  check(write_map_texture_cache(root/"odd.dds",odd_cache,error),"lossless odd top dimensions");
  check(read_map_texture_cache(root/"odd.dds",3,1,false,loaded,error)==MapCacheResult::Ready && !loaded.compressed,"lossless odd dimensions readback");
  MapModel gpu_model;mesh_probe::initialize_map_mip_fixture(gpu_model);
  const auto gpu_directory=parent/"gpu-cache";std::filesystem::create_directories(gpu_directory,ec);
  std::set<std::pair<size_t,MapMipOptions>> variants;
  for(const auto& primitive:gpu_model.primitives)for(unsigned slot=0;slot<5;++slot) {
    const auto index=primitive.material.textures[slot].image;
    if(index>=0)variants.emplace(static_cast<size_t>(index),map_mip_options(primitive.material,slot));
  }
  unsigned gpu_cached{},gpu_rejected{};
  for(const auto& [index,variant]:variants) {
    const auto& input=gpu_model.images[index];MapDecodedImage pixels;
    if(!decode_map_image(input,pixels,error)) {check(false,"shared GPU PNG decode");continue;}
    const auto cache_file=gpu_directory/(map_texture_cache_key(input,variant)+".dds");
    const bool is_srgb=variant.role==MapMipRole::BaseColor || variant.role==MapMipRole::Emissive;
    if(read_map_texture_cache(cache_file,pixels.width,pixels.height,is_srgb,loaded,error)==MapCacheResult::Ready) {++gpu_cached;continue;}
    if(!encode(build_map_mips(pixels,variant),variant,encoded,quality,error)) {
      ++gpu_rejected;check(encoded.levels.empty(),"shared GPU unsafe cache withheld");
      std::printf("texture_cook_gpu_rejected image=%zu role=%u reason=%s lossless_cache=1\n",index,unsigned(variant.role),error.c_str());
      encoded=lossless_map_texture_cache(build_map_mips(pixels,variant),is_srgb);
    }
    check(write_map_texture_cache(cache_file,encoded,error),"shared GPU cache published");++gpu_cached;
  }
  const auto& base_input=gpu_model.images[0];const auto base_options=map_mip_options(gpu_model.primitives[0].material,0);
  check(read_map_texture_cache(gpu_directory/(map_texture_cache_key(base_input,base_options)+".dds"),4,4,true,loaded,error)==MapCacheResult::Ready,
      "shared GPU base color is cooked BC7");
  std::printf("texture_cook_gpu_fixtures cached=%u quality_rejected=%u directory=%s\n",gpu_cached,gpu_rejected,gpu_directory.string().c_str());
  std::printf("map_texture_cook_checks=%u failed=%u runtime_encoder=0\n",checks,failed);
  return failed==0;
}
}
