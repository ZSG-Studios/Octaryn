#include "Encode.h"
#include <bc7decomp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace octaryn::client::rendering {
namespace {
MapCachedTexture read(const std::filesystem::path& path) {
  std::ifstream input(path,std::ios::binary);input.seekg(128);std::uint32_t format=0;input.read(reinterpret_cast<char*>(&format),4);
  MapCachedTexture result;std::string error;
  if(!input || read_map_texture_cache(path,0,0,format==29 || format==99,result,error)!=MapCacheResult::Ready)
    throw std::runtime_error(path.string()+": "+error);
  return result;
}
struct Metrics {
  double color_error{},alpha_error{};std::uint64_t pixels{},coverage[4]{};unsigned color_max{},alpha_max{};
  void pixel(const std::uint8_t* reference,const std::uint8_t* candidate) {
    ++pixels;for(unsigned channel=0;channel<4;++channel) {
      const auto delta=unsigned(std::abs(int(reference[channel])-int(candidate[channel])));
      if(channel==3) {alpha_error+=double(delta)*delta;alpha_max=std::max(alpha_max,delta);}
      else {color_error+=double(delta)*delta;color_max=std::max(color_max,delta);}
    }
    constexpr unsigned thresholds[]{32,64,128,192};
    for(unsigned i=0;i<4;++i)coverage[i]+=(reference[3]>=thresholds[i])!=(candidate[3]>=thresholds[i]);
  }
  double psnr()const {return color_error>0?10*std::log10(255.*255.*pixels*3/color_error):999.;}
};
Metrics compare(const MapCachedMip& reference,const MapCachedMip& candidate,bool compressed) {
  Metrics metrics;
  if(!compressed) {
    metrics.pixels=reference.blocks.size()/4;
    return metrics;
  }
  const auto columns=(candidate.width+3)/4,rows=(candidate.height+3)/4;
  for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<columns;++x) {
    bc7decomp::color_rgba pixels[16];
    if(!bc7decomp::unpack_bc7(candidate.blocks.data()+(size_t(y)*columns+x)*16,pixels))throw std::runtime_error("BC7 block decode failed");
    for(unsigned py=0;py<4 && y*4+py<candidate.height;++py)for(unsigned px=0;px<4 && x*4+px<candidate.width;++px)
      metrics.pixel(reference.blocks.data()+(size_t(y*4+py)*candidate.width+x*4+px)*4,pixels[py*4+px].m_comps);
  }
  return metrics;
}
}
bool compare_map_texture_caches(const std::filesystem::path& reference_path,const std::filesystem::path& candidate_path,
    const std::filesystem::path& output) {
  if(!output.parent_path().empty())std::filesystem::create_directories(output.parent_path());
  std::ofstream csv(output);csv<<std::setprecision(10)<<"key,mip,width,height,compressed,pixels,rgb_rmse_srgb_codes,rgb_psnr_srgb_codes,rgb_max_error,alpha_rmse,alpha_max_error,alpha_flip_32,alpha_flip_64,alpha_flip_128,alpha_flip_192\n";
  unsigned files=0,compressed=0;std::uint64_t reference_bytes=0,candidate_bytes=0;
  double minimum_psnr=999;
  for(const auto& file:std::filesystem::directory_iterator(reference_path)) {
    if(file.path().extension()!=".dds")continue;
    const auto reference=read(file.path()),candidate=read(candidate_path/file.path().filename());
    if(reference.compressed || reference.srgb!=candidate.srgb || reference.opaque!=candidate.opaque || reference.levels.size()!=candidate.levels.size())
      throw std::runtime_error("reference/candidate metadata differs");
    if(candidate.compressed && !candidate.srgb)throw std::runtime_error("normal/data texture unexpectedly compressed");
    for(size_t level=0;level<reference.levels.size();++level) {
      const auto& a=reference.levels[level];const auto& b=candidate.levels[level];
      if(a.width!=b.width || a.height!=b.height)throw std::runtime_error("mip dimensions differ");
      if(!candidate.compressed && a.blocks!=b.blocks)throw std::runtime_error("lossless data/normal mip differs");
      const auto metrics=compare(a,b,candidate.compressed);reference_bytes+=a.blocks.size();candidate_bytes+=b.blocks.size();minimum_psnr=std::min(minimum_psnr,metrics.psnr());
      csv<<file.path().stem().string()<<','<<level<<','<<a.width<<','<<a.height<<','<<candidate.compressed<<','<<metrics.pixels<<','
         <<std::sqrt(metrics.color_error/(metrics.pixels*3))<<','<<metrics.psnr()<<','<<metrics.color_max<<','
         <<std::sqrt(metrics.alpha_error/metrics.pixels)<<','<<metrics.alpha_max;
      for(auto count:metrics.coverage)csv<<','<<count;csv<<'\n';
    }
    ++files;compressed+=candidate.compressed;
  }
  csv.close();if(!csv || !files)throw std::runtime_error("empty or unwritable comparison report");
  std::printf("map_texture_compare valid=1 files=%u compressed=%u reference_bytes=%llu candidate_bytes=%llu minimum_mip_psnr=%.4f visual_qualification=required\n",
      files,compressed,reference_bytes,candidate_bytes,minimum_psnr);return true;
}
}
