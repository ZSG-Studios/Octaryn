#include "Encode.h"
#include <bc7enc.h>
#include <bc7decomp.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace map_texture_cook {
namespace {
float angle(const std::uint8_t* a,const std::uint8_t* b) {
  float dot{},aa{},bb{};
  for(unsigned c=0;c<3;++c) {const float x=a[c]/127.5f-1,y=b[c]/127.5f-1;dot+=x*y;aa+=x*x;bb+=y*y;}
  if(aa<1e-6f || bb<1e-6f)return 180;
  return std::acos(std::clamp(dot/std::sqrt(aa*bb),-1.f,1.f))*57.2957795f;
}
}
bool encode(const std::vector<MapDecodedImage>& levels,const MapMipOptions& options,
    MapCachedTexture& output,Quality& quality,std::string& error) {
  output={};quality={};error.clear();
  if(levels.empty()) {error="empty mip chain";return false;}
  if(levels.front().width%4 || levels.front().height%4) {
    error="BC7 top-level dimensions must be multiples of four for DX12";return false;
  }
  bc7enc_compress_block_init();bc7enc_compress_block_params params;
  bc7enc_compress_block_params_init(&params);params.m_uber_level=2;
  // Linear channel error protects normal/data channels; no RDO entropy tradeoff.
  bc7enc_compress_block_params_init_linear_weights(&params);
  const bool srgb=options.role==MapMipRole::BaseColor || options.role==MapMipRole::Emissive;
  MapCachedTexture encoded;encoded.srgb=srgb;
  for(const auto& level:levels) {
    if(!level.width || !level.height || level.rgba.size()!=static_cast<size_t>(level.width)*level.height*4) {
      error="invalid mip input";return false;
    }
    MapCachedMip mip{level.width,level.height,{}};
    const auto blocks_x=(level.width+3)/4,blocks_y=(level.height+3)/4;
    mip.blocks.resize(static_cast<size_t>(blocks_x)*blocks_y*16);
    for(unsigned by=0;by<blocks_y;++by)for(unsigned bx=0;bx<blocks_x;++bx) {
      color_rgba input[16];bc7decomp::color_rgba decoded[16];
      for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x) {
        const auto sx=std::min(level.width-1,bx*4+x),sy=std::min(level.height-1,by*4+y);
        std::memcpy(input[y*4+x].m_c,&level.rgba[(static_cast<size_t>(sy)*level.width+sx)*4],4);
      }
      auto* block=&mip.blocks[(static_cast<size_t>(by)*blocks_x+bx)*16];
      bc7enc_compress_block(block,input,&params);
      if(!bc7decomp::unpack_bc7(block,decoded)) {error="BC7 verification decode failed";return false;}
      for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x) {
        if(bx*4+x>=level.width || by*4+y>=level.height)continue;
        const auto* a=input[y*4+x].m_c;const auto* b=decoded[y*4+x].m_comps;++quality.pixels;
        for(unsigned c=0;c<4;++c) {
          const unsigned delta=static_cast<unsigned>(std::abs(int(a[c])-int(b[c])));
          quality.maximum_error=std::max(quality.maximum_error,delta);quality.squared_error+=delta*delta;
          if(delta>24) {error="BC7 quality gate rejected variant";return false;}
        }
        if(options.preserve_coverage && ((a[3]/255.f*options.alpha_factor>=options.alpha_cutoff)!=
            (b[3]/255.f*options.alpha_factor>=options.alpha_cutoff))) {
          ++quality.mask_changed;error="BC7 quality gate rejected variant";return false;
        }
        if(options.role==MapMipRole::Normal) {
          const auto degrees=angle(a,b);quality.normal_degrees+=degrees;
          quality.maximum_normal_degrees=std::max(quality.maximum_normal_degrees,degrees);
          if(degrees>5) {error="BC7 quality gate rejected variant";return false;}
        }
        // Fully opaque authored pixels must not become subtly transparent.
        if(options.alpha_weighted && a[3]==255 && b[3]!=255) {error="opaque alpha changed";return false;}
      }
    }
    encoded.levels.push_back(std::move(mip));
  }
  if(quality.mask_changed || quality.maximum_error>24 || std::sqrt(quality.squared_error/(quality.pixels*4.0))>5 ||
      quality.maximum_normal_degrees>5 || quality.normal_degrees/std::max(quality.pixels,1u)>1.5) {
    error="BC7 quality gate rejected variant";return false;
  }
  output=std::move(encoded);return true;
}
}
