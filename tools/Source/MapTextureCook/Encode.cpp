#include "Encode.h"
#include <bc7enc.h>
#include <algorithm>
#include <array>
#include <mutex>

namespace octaryn::client::rendering {
MapCachedTexture encode_map_bc7(const std::vector<MapDecodedImage>& levels,bool srgb) {
  static std::once_flag initialized;
  std::call_once(initialized,bc7enc_compress_block_init);
  MapCachedTexture output;output.srgb=srgb;output.opaque=true;
  bc7enc_compress_block_params params{};bc7enc_compress_block_params_init(&params);
  params.m_uber_level=4;params.m_mode17_partition_estimation_filterbank=false;
  if(!srgb)bc7enc_compress_block_params_init_linear_weights(&params);
  for(const auto& source:levels) {
    const auto columns=(source.width+3)/4,rows=(source.height+3)/4;
    MapCachedMip mip{source.width,source.height,{}};mip.blocks.resize(size_t(columns)*rows*16);
    for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<columns;++x) {
      std::array<std::uint8_t,64> pixels{};
      for(unsigned py=0;py<4;++py)for(unsigned px=0;px<4;++px) {
        const auto offset=(size_t(std::min(y*4+py,source.height-1))*source.width+std::min(x*4+px,source.width-1))*4;
        std::copy_n(source.rgba.data()+offset,4,pixels.data()+(py*4+px)*4);
        if(source.rgba[offset+3]!=255)output.opaque=false;
      }
      bc7enc_compress_block(mip.blocks.data()+(size_t(y)*columns+x)*16,pixels.data(),&params);
    }
    output.levels.push_back(std::move(mip));
  }
  return output;
}
}
