#include "Probe.h"
#include "AtlasPixels.h"
#include <algorithm>
#include <cmath>

namespace mesh_probe {
void atlas_mip_cases() {
  std::array<Uint8,8> low_alpha{255,255,255,64,0,0,0,0};
  atlas_preserve_alpha_coverage(low_alpha.data(),2,.5f);
  require(atlas_alpha_coverage(low_alpha.data(),2)==.5f && low_alpha[3]>=90 && low_alpha[7]==0,
      "required alpha correction was invalidated by byte rounding");
  std::array<Uint8,4> one_pixel{255,255,255,128};
  atlas_preserve_alpha_coverage(one_pixel.data(),1,.5f);
  require(atlas_alpha_coverage(one_pixel.data(),1)==1 && one_pixel[3]>=90,
      "unrepresentable half coverage must remain a covered one-pixel mip");
  std::array<Uint8,16> mixed{0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255};
  std::array<Uint8,4> average{};
  atlas_downsample_tile_rgba(mixed.data(),2,average.data());
  require(average[0]>=187 && average[0]<=189 && average[3]==255,"albedo mip is not a linear-light average");
  mixed={255,0,0,255,0,0,255,0,0,0,255,0,0,0,255,0};
  atlas_downsample_tile_rgba(mixed.data(),2,average.data());
  require(average[0]==255 && average[1]==0 && average[2]==0 && average[3]==64,
      "transparent RGB contaminated premultiplied albedo mip");
  unsigned levels=0;
  for(const auto kind:{ATLAS_MIP_ALBEDO,ATLAS_MIP_LABPBR_NORMAL,ATLAS_MIP_LABPBR_SPECULAR}) {
    std::vector<Uint8> current(32*32*4),scratch(current.size()),packed(1365*4);
    const std::array<Uint8,4> value=kind==ATLAS_MIP_ALBEDO?std::array<Uint8,4>{40,120,220,255}:
        kind==ATLAS_MIP_LABPBR_NORMAL?std::array<Uint8,4>{128,128,255,255}:std::array<Uint8,4>{70,230,65,255};
    for(std::size_t i=0;i<current.size();i+=4)std::copy(value.begin(),value.end(),current.data()+i);
    Uint32 bytes=0;atlas_pack_layer_mips(packed.data(),&bytes,current.data(),scratch.data(),6,kind);
    require(bytes==packed.size(),"atlas mip byte layout mismatch");
    for(std::size_t i=0;i<packed.size();++i)require(packed[i]==value[i%4],"flat material or no-emission sentinel changed across mips");
    levels+=6;
  }
  // Exercise encoded-channel category majorities at the first actual 2x2 reduction.
  std::vector<Uint8> current(32*32*4),scratch(current.size()),packed(1365*4);
  for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
    const auto i=(y*32+x)*4;const bool minority=(x%2==0 && y%2==0);
    current[i]=100;current[i+1]=minority?229:230;current[i+2]=minority?64:65;current[i+3]=x%2?128:255;
  }
  Uint32 bytes=0;atlas_pack_layer_mips(packed.data(),&bytes,current.data(),scratch.data(),6,ATLAS_MIP_LABPBR_SPECULAR);
  for(unsigned i=32*32*4;i<bytes;i+=4)
    require(packed[i+1]==230 && packed[i+2]==65 && packed[i+3]==128,"LabPBR mip blended categorical channels or emission sentinel");
  // A half-tile cutout has exactly representable coverage until the final 1x1 mip.
  for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
    const auto i=(y*32+x)*4;current[i]=80;current[i+1]=180;current[i+2]=40;current[i+3]=x<16?255:0;
  }
  bytes=0;atlas_pack_layer_mips(packed.data(),&bytes,current.data(),scratch.data(),6,ATLAS_MIP_ALBEDO);
  unsigned offset=0;
  for(unsigned size=32;size>=2;size/=2) {
    require(std::abs(atlas_alpha_coverage(packed.data()+offset,int(size*size))-.5f)<.00001f,"cutout mip lost representable alpha coverage");
    for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x)
      require(packed[offset+(y*size+x)*4+3]==(x<size/2?255:0),
          "already-correct cutout coverage destructively rescaled alpha");
    offset+=size*size*4;
  }
  std::printf("atlas_mips=passed flat_levels=%u linear_light=1 alpha_weighting=1 coverage=1 labpbr_categories=1 one_pixel_target=.5 one_pixel_actual=1\n",levels);
}
}
