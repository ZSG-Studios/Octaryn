#include "Encode.h"
#include <bc7decomp.h>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace octaryn::client::rendering {
bool test_map_texture_cook(const std::filesystem::path& root) {
  const auto require=[](bool okay,const char* reason) {if(!okay)throw std::runtime_error(reason);};
  std::filesystem::create_directories(root);
  require(map_texture_digest({})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA256 fixture");
  MapDecodedImage source{8,8,{}};source.rgba.resize(8*8*4);
  for(size_t i=0;i<64;++i) {source.rgba[i*4]=64;source.rgba[i*4+1]=128;source.rgba[i*4+2]=192;source.rgba[i*4+3]=255;}
  const auto levels=build_map_mips(source,{});
  for(bool compressed:{false,true}) {
    auto texture=compressed?encode_map_bc7(levels,true):lossless_map_texture_cache(levels,true);
    const auto path=root/(compressed?"bc7.dds":"rgba.dds");std::string error;MapCachedTexture result;
    require(write_map_texture_cache(path,texture,error),"write cache");
    require(read_map_texture_cache(path,0,0,true,result,error)==MapCacheResult::Ready,"metadata-only cache read");
    require(result.opaque && result.levels.size()==4 && result.compressed==compressed,"cache metadata");
    require(read_map_texture_cache(path,4,4,true,result,error)==MapCacheResult::Invalid,"dimension mismatch rejection");
    require(read_map_texture_cache(path,0,0,false,result,error)==MapCacheResult::Invalid,"color-space rejection");
    if(compressed) {
      bc7decomp::color_rgba pixels[16];
      require(bc7decomp::unpack_bc7(texture.levels[0].blocks.data(),pixels),"BC7 decode");
      for(const auto& pixel:pixels)for(unsigned c=0;c<4;++c)
        require(std::abs(int(pixel.m_comps[c])-int(source.rgba[c]))<=3,"BC7 flat color fidelity");
    } else require(texture.levels[0].blocks==source.rgba,"lossless base image parity");
    std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);file.seekp(149);file.put(0);file.close();
    require(read_map_texture_cache(path,0,0,true,result,error)==MapCacheResult::Invalid,"corruption rejection");
  }
  source.rgba[3]=0;
  require(!lossless_map_texture_cache(build_map_mips(source,{}),true).opaque,"source alpha metadata");
  const auto reference=root/"reference",candidate=root/"candidate";
  std::filesystem::create_directories(reference);std::filesystem::create_directories(candidate);
  std::string error;const auto lossless=lossless_map_texture_cache(levels,true);
  require(write_map_texture_cache(reference/"color.dds",lossless,error) &&
      write_map_texture_cache(candidate/"color.dds",encode_map_bc7(levels,true),error),"comparison color fixtures");
  auto data=lossless_map_texture_cache(levels,false);
  require(write_map_texture_cache(reference/"data.dds",data,error) &&
      write_map_texture_cache(candidate/"data.dds",data,error),"comparison data fixtures");
  require(compare_map_texture_caches(reference,candidate,root/"comparison.csv"),"comparison report");
  data.levels[0].blocks[0]^=1;
  require(write_map_texture_cache(candidate/"data.dds",data,error),"altered comparison data");
  bool rejected=false;try {compare_map_texture_caches(reference,candidate,root/"invalid.csv");}catch(const std::exception&) {rejected=true;}
  require(rejected,"data mip difference silently accepted");
  std::printf("map_texture_cook_tests passed=1\n");return true;
}
}
