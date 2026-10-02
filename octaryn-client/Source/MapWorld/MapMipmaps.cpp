#include "MapMipmaps.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace octaryn::client::rendering {
namespace {
float linear(std::uint8_t encoded) {
  static const auto values=[] {
    std::array<float,256> table{};
    for(unsigned i=0;i<256;++i) {const float v=i/255.f;table[i]=v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
    return table;
  }();
  return values[encoded];
}
float srgb(float v) {return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.f/2.4f)-.055f;}
std::uint8_t byte(float v) {return static_cast<std::uint8_t>(std::clamp(std::lround(v*255),0l,255l));}
bool color(MapMipRole role) {return role==MapMipRole::BaseColor || role==MapMipRole::Emissive;}

// Adapted from AtlasPixels: fill transparent border RGB without changing alpha.
void dilate(MapDecodedImage& image) {
  {
    const auto source=image.rgba;
    for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x) {
      const auto offset=(static_cast<size_t>(y)*image.width+x)*4;
      if(source[offset+3])continue;
      float rgb[3]{},weight{};
      for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
        const int px=static_cast<int>(x)+dx,py=static_cast<int>(y)+dy;
        if(px<0 || py<0 || px>=static_cast<int>(image.width) || py>=static_cast<int>(image.height))continue;
        const auto index=(static_cast<size_t>(py)*image.width+px)*4;
        const float alpha=source[index+3]/255.f;
        for(unsigned c=0;c<3;++c)rgb[c]+=linear(source[index+c])*alpha;
        weight+=alpha;
      }
      if(weight>0)for(unsigned c=0;c<3;++c)image.rgba[offset+c]=byte(srgb(rgb[c]/weight));
    }
  }
}
double coverage(const MapDecodedImage& image,const MapMipOptions& options,float scale=1) {
  size_t passed{};
  for(size_t i=3;i<image.rgba.size();i+=4)
    if(byte(std::min(1.f,image.rgba[i]/255.f*scale))/255.f*options.alpha_factor>=options.alpha_cutoff)++passed;
  return static_cast<double>(passed)/(image.rgba.size()/4);
}
void preserve(MapDecodedImage& image,const MapMipOptions& options,double target) {
  if(options.alpha_factor<=0 || options.alpha_cutoff<=0 || options.alpha_cutoff>options.alpha_factor)return;
  std::array<size_t,256> histogram{};
  for(size_t i=3;i<image.rgba.size();i+=4)++histogram[image.rgba[i]];
  const auto scaled_coverage=[&](float scale) {
    size_t passed{};
    for(unsigned alpha=0;alpha<256;++alpha)
      if(byte(std::min(1.f,alpha/255.f*scale))/255.f*options.alpha_factor>=options.alpha_cutoff)passed+=histogram[alpha];
    return static_cast<double>(passed)/(image.rgba.size()/4);
  };
  float low=0,high=256,best=1;
  double best_coverage=scaled_coverage(1),best_error=std::abs(best_coverage-target);
  if(best_error==0)return;
  // Discrete pixel coverage cannot always match exactly; ties retain silhouettes.
  for(unsigned iteration=0;iteration<20;++iteration) {
    const float scale=(low+high)*.5f;
    const double actual=scaled_coverage(scale),error=std::abs(actual-target);
    if(error<best_error || (error==best_error && actual>=target &&
        (best_coverage<target || scale<best))) {best_error=error;best_coverage=actual;best=scale;}
    if(actual<target)low=scale;else high=scale;
  }
  for(size_t i=3;i<image.rgba.size();i+=4)image.rgba[i]=byte(std::min(1.f,image.rgba[i]/255.f*best));
}
MapDecodedImage downsample(const MapDecodedImage& source,const MapMipOptions& options) {
  MapDecodedImage out;out.width=std::max(1u,source.width/2);out.height=std::max(1u,source.height/2);
  out.rgba.resize(static_cast<size_t>(out.width)*out.height*4);
  for(unsigned y=0;y<out.height;++y)for(unsigned x=0;x<out.width;++x) {
    float sum[4]{},weights{},samples{};
    for(unsigned sy=y*source.height/out.height;sy<(y+1)*source.height/out.height;++sy)
      for(unsigned sx=x*source.width/out.width;sx<(x+1)*source.width/out.width;++sx) {
        const auto* pixel=&source.rgba[(static_cast<size_t>(sy)*source.width+sx)*4];
        const float alpha=pixel[3]/255.f,weight=options.alpha_weighted?alpha:1.f;
        for(unsigned c=0;c<3;++c) {
          float value=pixel[c]/255.f;
          if(color(options.role))value=linear(pixel[c]);
          else if(options.role==MapMipRole::Normal)value=value*2-1;
          else if(options.role==MapMipRole::MetalRough && c==1)value*=value;
          sum[c]+=value*weight;
        }
        sum[3]+=alpha;weights+=weight;++samples;
      }
    auto* pixel=&out.rgba[(static_cast<size_t>(y)*out.width+x)*4];
    for(unsigned c=0;c<3;++c)sum[c]/=std::max(weights,1e-8f);
    if(options.role==MapMipRole::Normal) {
      const float length=std::sqrt(sum[0]*sum[0]+sum[1]*sum[1]+sum[2]*sum[2]);
      if(length>1e-5f)for(unsigned c=0;c<3;++c)sum[c]/=length;
      else {sum[0]=sum[1]=0;sum[2]=1;}
    }
    for(unsigned c=0;c<3;++c) {
      float value=sum[c];
      if(color(options.role))value=srgb(value);
      else if(options.role==MapMipRole::Normal)value=value*.5f+.5f;
      else if(options.role==MapMipRole::MetalRough && c==1)value=std::sqrt(std::max(0.f,value));
      pixel[c]=byte(value);
    }
    pixel[3]=byte(sum[3]/samples);
  }
  return out;
}
}
MapMipOptions map_mip_options(const MapMaterial& material,unsigned role) {
  MapMipOptions options;options.role=static_cast<MapMipRole>(role<5?role:((role-5)%2?2:0));
  options.alpha_weighted=role==0 && material.alpha_mode!=MapAlphaMode::Opaque;
  options.preserve_coverage=role==0 && material.alpha_mode==MapAlphaMode::Mask;
  if(options.preserve_coverage) {options.alpha_cutoff=material.alpha_cutoff;options.alpha_factor=material.base_color[3];}
  return options;
}
std::vector<MapDecodedImage> build_map_mips(const MapDecodedImage& source,const MapMipOptions& options) {
  if(!source.width || !source.height || source.rgba.size()!=static_cast<size_t>(source.width)*source.height*4)return {};
  std::vector<MapDecodedImage> levels{source};
  const double target=options.preserve_coverage?coverage(source,options):0;
  if(options.alpha_weighted)dilate(levels.front());
  while(levels.back().width>1 || levels.back().height>1) {
    auto next=downsample(levels.back(),options);
    if(options.alpha_weighted)dilate(next);
    if(options.preserve_coverage)preserve(next,options,target);
    levels.push_back(std::move(next));
  }
  return levels;
}
}
