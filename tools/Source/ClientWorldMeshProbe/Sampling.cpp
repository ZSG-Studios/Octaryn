#include "Probe.h"
#include "AtlasInternal.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
namespace mesh_probe {
namespace {
float decode(const Image& image,std::size_t pixel,unsigned target) {
  std::uint32_t bits=0;
  for(unsigned c=0;c<4;++c) {
    unsigned byte{};
    if(target<2) {
      std::uint16_t value{};std::memcpy(&value,image.mrt[target].data()+(pixel*4+c)*2,2);
      const unsigned exponent=(value>>10)&31,mantissa=value&1023;
      byte=static_cast<unsigned>(std::lround(std::ldexp(float(exponent?mantissa+1024:mantissa),exponent?int(exponent)-25:-24)));
    } else byte=image.mrt[target][pixel*4+c];
    bits|=byte<<(c*8);
  }
  return std::bit_cast<float>(bits);
}
float color_component(const Image& image,std::size_t pixel,unsigned channel,float* ulp=nullptr) {
  std::uint16_t bits{};std::memcpy(&bits,image.mrt[0].data()+(pixel*4+channel)*2,2);
  const int exponent=(bits>>10)&31,mantissa=bits&1023;
  if(ulp)*ulp=std::ldexp(1.f,exponent?exponent-25:-24);
  return std::ldexp(float(exponent?mantissa+1024:mantissa),exponent?exponent-25:-24)*(bits&32768?-1.f:1.f);
}
}
SamplingBoundaries sampling_boundaries(Fixture& f,const Mesh& merged,const Mesh& units,const WorldCamera& camera,bool pbr,bool pom,
                                      const std::vector<bool>& needs_albedo,const Image& rendered_a,const Image& rendered_b) {
  auto& r=f.renderer;auto opaque=r.raster_pipeline,sprite=r.sprite_pipeline;
  std::vector<float> texels(29*1365*4);std::vector<rhi::SubresourceData> data;std::size_t offset=0;
  for(unsigned layer=0;layer<29;++layer)for(unsigned mip=0,size=32;mip<6;++mip,size/=2) {
    const auto count=size*size*4;std::fill_n(texels.data()+offset,count,float(mip));
    data.push_back({texels.data()+offset,size*16,size*size*16});offset+=count;
  }
  rhi::TextureDesc texture{};texture.type=rhi::TextureType::Texture2DArray;texture.size={32,32,1};
  texture.arrayLength=29;texture.mipCount=6;texture.format=rhi::Format::RGBA32Float;
  texture.usage=rhi::TextureUsage::ShaderResource;texture.defaultState=rhi::ResourceState::ShaderResource;
  Slang::ComPtr<rhi::ITexture> mip_texture;Slang::ComPtr<rhi::ITextureView> mip_view;
  checked(r.device->createTexture(texture,data.data(),mip_texture.writeRef()),"sampling mip calibration texture");
  checked(mip_texture->getDefaultView(mip_view.writeRef()),"sampling mip calibration view");
  Slang::ComPtr<rhi::IShaderProgram> program;const char* entries[]={"vertex_main","sampling_diagnostic"};
  require(create_rhi_program(r.device,"octaryn-client/Shaders/Voxel/SamplingDiagnostic.slang",entries,2,program),"sampling diagnostic program");
  std::array<rhi::ColorTargetDesc,4> targets{};
  for(unsigned i=0;i<4;++i)targets[i].format=world_gbuffer_formats[i];
  rhi::RenderPipelineDesc desc{};desc.program=program;desc.targets=targets.data();desc.targetCount=4;
  desc.depthStencil=opaque->getDesc().depthStencil;desc.rasterizer=opaque->getDesc().rasterizer;
  desc.label="world_mesh_sampling";
  checked(r.device->createRenderPipeline(desc,r.raster_pipeline.writeRef()),"sampling diagnostic pipeline");
  desc.rasterizer=sprite->getDesc().rasterizer;
  checked(r.device->createRenderPipeline(desc,r.sprite_pipeline.writeRef()),"sprite diagnostic pipeline");
  auto specular=r.atlas->views[2];r.atlas->views[2]=mip_view;
  const auto a=f.render(merged.gpu,camera,pbr,pom),b=f.render(units.gpu,camera,pbr,pom);
  entries[1]="sampling_derivatives";
  require(create_rhi_program(r.device,"octaryn-client/Shaders/Voxel/SamplingDiagnostic.slang",entries,2,program),"sampling derivative program");
  desc.program=program;desc.rasterizer=opaque->getDesc().rasterizer;
  checked(r.device->createRenderPipeline(desc,r.raster_pipeline.writeRef()),"sampling derivative pipeline");
  desc.rasterizer=sprite->getDesc().rasterizer;
  checked(r.device->createRenderPipeline(desc,r.sprite_pipeline.writeRef()),"sprite derivative pipeline");
  const auto gradients_a=f.render(merged.gpu,camera,pbr,pom),gradients_b=f.render(units.gpu,camera,pbr,pom);
  r.atlas->views[2]=specular;
  r.raster_pipeline=opaque;r.sprite_pipeline=sprite;
  SamplingBoundaries result;
  result.albedo.resize(Fixture::Size*Fixture::Size);result.material.resize(result.albedo.size());
  require(needs_albedo.size()==result.albedo.size(),"sampling diagnostic mask size");
  constexpr float pixel_envelope=1.f/256,lod_epsilon=.01f;
  unsigned boundaries=0;
  std::vector<std::array<float,8>> reference_inputs;std::vector<std::size_t> reference_pixels;
  for(std::size_t i=0;i<result.albedo.size();++i) {
    if(a.depth[i]>=1 || b.depth[i]>=1)continue;
    const float au=decode(a,i,0),av=decode(a,i,1),bu=decode(b,i,0),bv=decode(b,i,1);
    const float alod=std::log2(std::max(decode(a,i,3),.000001f));
    const float blod=std::log2(std::max(decode(b,i,3),.000001f));
    const int amip=int(std::lround(decode(a,i,2))),bmip=int(std::lround(decode(b,i,2)));
    require(amip>=0 && amip<=5 && bmip>=0 && bmip<=5,"invalid measured mip selection");
    // Integer UV offsets are the same periodic sample. Boundaries must actually
    // straddle a nearest texel/mip choice within a tiny measured precision envelope.
    const float du=std::remainder(au-bu,1.f),dv=std::remainder(av-bv,1.f);
    const float gradient=std::max(decode(a,i,3),decode(b,i,3))/32;
    const float uv_epsilon=gradient*pixel_envelope+32*1.1920929e-7f;
    if(needs_albedo[i])std::printf("world_mesh_albedo_precision pixel=%zu,%zu uv_a=%.9g,%.9g uv_b=%.9g,%.9g delta=%.9g,%.9g epsilon=%.9g footprint_a=%.9g footprint_b=%.9g point_mip=%d,%d\n",
        i%Fixture::Size,i/Fixture::Size,au,av,bu,bv,du,dv,uv_epsilon,decode(a,i,3),decode(b,i,3),amip,bmip);
    if(std::abs(du)>uv_epsilon || std::abs(dv)>uv_epsilon)continue;
    if(needs_albedo[i] && gradient*32>=1 && gradients_a.depth[i]<1 && gradients_b.depth[i]<1 &&
        color_component(rendered_a,i,3)==color_component(rendered_b,i,3)) {
      std::array<float,8> input_a{au,av},input_b{bu,bv};bool stable=true;float maximum_delta=0;
      for(unsigned component=0;component<4;++component) {
        const float ga=decode(gradients_a,i,component),gb=decode(gradients_b,i,component);
        input_a[component+2]=ga;input_b[component+2]=gb;
        maximum_delta=std::max(maximum_delta,std::abs(ga-gb));
        stable &= std::isfinite(ga) && std::isfinite(gb) && std::abs(ga-gb)<=uv_epsilon;
      }
      unsigned voxel=0;
      for(unsigned byte=0;byte<4;++byte)voxel|=unsigned(rendered_a.mrt[2][i*4+byte])<<(byte*8);
      input_a[6]=input_b[6]=float(voxel);
      std::printf("world_mesh_gradient_precision pixel=%zu,%zu delta_max=%.9g envelope=%.9g stable=%u\n",
          i%Fixture::Size,i/Fixture::Size,maximum_delta,uv_epsilon,stable);
      if(stable) {reference_pixels.push_back(i);reference_inputs.push_back(input_a);reference_inputs.push_back(input_b);}
    }
    if(amip!=bmip)result.material[i]=std::abs(alod-blod)<=lod_epsilon && std::abs(amip-bmip)==1;
    else {
      const float size=float(32>>amip);
      const auto texel=[&](float value){return int(std::floor((value-std::floor(value))*size));};
      result.material[i]=texel(au)!=texel(bu) || texel(av)!=texel(bv);
      // Filtered minification is continuous: a point-data boundary cannot excuse
      // an unrelated albedo mismatch. Only the magnified point component jumps.
      result.albedo[i]=amip==0 && gradient*32<1 && result.material[i];
    }
    if(result.material[i])++boundaries;
  }
  const auto references=sampling_reference(f,reference_inputs);
  for(std::size_t index=0;index<reference_pixels.size();++index) {
    const auto pixel=reference_pixels[index];bool reproduced=true;float max_error=0;
    for(unsigned side=0;side<2;++side)for(unsigned channel=0;channel<3;++channel) {
      float ulp=0;const float stored=color_component(side?rendered_b:rendered_a,pixel,channel,&ulp);
      const float sampled=references[index*2+side][channel],error=std::abs(stored-sampled);
      reproduced &= std::isfinite(sampled) && error<=ulp;max_error=std::max(max_error,error);
    }
    std::printf("world_mesh_filter_reference pixel=%zu,%zu reproduced=%u error_max=%g\n",
        pixel%Fixture::Size,pixel/Fixture::Size,reproduced,max_error);
    result.albedo[pixel]=reproduced;
  }
  std::printf("world_mesh_sampling classified_boundaries=%u pixel_envelope=%g lod_envelope=%g\n",boundaries,pixel_envelope,lod_epsilon);
  return result;
}
}
