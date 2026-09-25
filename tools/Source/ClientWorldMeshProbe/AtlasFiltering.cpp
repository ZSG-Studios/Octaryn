#include "Probe.h"
#include "AtlasInternal.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>

namespace mesh_probe {
void atlas_filtering_cases(Fixture& f) {
  auto& r=f.renderer;
  const auto& point=r.atlas->cutout->getDesc();
  const auto& linear=r.atlas->linear->getDesc();
  const auto& sprite=r.atlas->sprite->getDesc();
  require(point.minFilter==rhi::TextureFilteringMode::Point && point.maxAnisotropy==1,
      "encoded material data requires point filtering");
  require(linear.minFilter==rhi::TextureFilteringMode::Linear && linear.mipFilter==rhi::TextureFilteringMode::Linear &&
      linear.maxAnisotropy==8 && linear.maxLOD==5,"terrain sampler must cover all mips with anisotropic trilinear filtering");
  require(sprite.addressU==rhi::TextureAddressingMode::ClampToEdge && sprite.addressV==rhi::TextureAddressingMode::ClampToEdge,
      "sprite sampler must clamp coarse mip footprints");
  std::vector<Uint8> alpha_pixels(32*32*4),alpha_scratch(alpha_pixels.size()),alpha_mips(1365*4);
  for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
    const auto i=(y*32+x)*4;
    alpha_pixels[i]=alpha_pixels[i+1]=alpha_pixels[i+2]=255;alpha_pixels[i+3]=x<16?255:0;
  }
  Uint32 alpha_bytes=0;
  atlas_pack_layer_mips(alpha_mips.data(),&alpha_bytes,alpha_pixels.data(),alpha_scratch.data(),6,ATLAS_MIP_ALBEDO);
  require(alpha_bytes==alpha_mips.size(),"filtered alpha mip layout");
  std::vector<float> pixels(4*1365*4);std::vector<rhi::SubresourceData> data;std::size_t offset=0;
  for(unsigned layer=0;layer<4;++layer)for(unsigned mip=0,size=32;mip<6;++mip,size/=2) {
    for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x) {
      const float value=layer==0?(mip?.5f:float((x+y)&1)):
          layer==1?float(mip)/5:(size==1?.5f:float(x>=size/2));
      const auto index=offset+(y*size+x)*4;
      for(unsigned channel=0;channel<3;++channel)pixels[index+channel]=value;
      pixels[index+3]=layer==3?float(alpha_mips[index-3*1365*4+3])/255:1;
    }
    data.push_back({pixels.data()+offset,size*16,size*size*16});offset+=size*size*4;
  }
  rhi::TextureDesc desc{};desc.type=rhi::TextureType::Texture2DArray;desc.size={32,32,1};
  desc.arrayLength=4;desc.mipCount=6;desc.format=rhi::Format::RGBA32Float;
  desc.usage=rhi::TextureUsage::ShaderResource;desc.defaultState=rhi::ResourceState::ShaderResource;
  Slang::ComPtr<rhi::ITexture> texture;Slang::ComPtr<rhi::ITextureView> view;
  checked(r.device->createTexture(desc,data.data(),texture.writeRef()),"filter calibration texture");
  checked(texture->getDefaultView(view.writeRef()),"filter calibration view");
  using Sample=std::array<float,8>;std::vector<Sample> inputs;
  const auto add=[&](float u,float v,float footprint,unsigned layer,unsigned mode=0) {
    inputs.push_back({u,v,footprint/32,0,0,footprint/32,float(layer<<3),float(mode)});
  };
  add(2.5f/32,2.5f/32,.25f,0);add(3.5f/32,2.5f/32,.25f,0);
  for(unsigned i=0;i<17;++i)add((float(i)+.13f)/32,.213f,8,0);
  const auto ramp_begin=inputs.size();
  for(unsigned i=0;i<=80;++i)add(.371f,.219f,std::exp2(float(i)/16),1);
  const auto plain_begin=inputs.size();
  for(unsigned i=0;i<=80;++i)add(.371f,.219f,std::exp2(float(i)/16),1,3);
  const auto bias_begin=inputs.size();
  for(unsigned mip=1;mip<=5;++mip)add(.371f,.219f,float(1u<<mip),1,4);
  const auto transition_begin=inputs.size();
  for(unsigned i=0;i<=28;++i)add(2.99f/32,2.99f/32,.4f+float(i)*.025f,0);
  const auto sprite_begin=inputs.size();add(0,.5f,4,2,1);add(1,.5f,4,2,1);
  const auto point_begin=inputs.size();add(.37f,.21f,4,1,2);
  // Strong anisotropy in either screen direction must still average the fine checker.
  const auto oblique_begin=inputs.size();
  inputs.push_back({.123f,.321f,16.f/32,0,0,1.f/32,0,0});
  inputs.push_back({.123f,.321f,1.f/32,0,0,16.f/32,0,0});
  const auto alpha_begin=inputs.size();constexpr unsigned alpha_samples=1024;
  for(unsigned mode=0;mode<2;++mode)for(unsigned mip=0;mip<=4;++mip)
    for(unsigned x=0;x<alpha_samples;++x)add((float(x)+.5f)/alpha_samples,.5f,float(1u<<mip),3,mode);
  std::vector<std::array<float,4>> output(inputs.size());
  auto input=buffer(r,inputs.data(),inputs.size()*sizeof(Sample),16,rhi::BufferUsage::ShaderResource);
  rhi::BufferDesc result_desc{};result_desc.size=output.size()*16;result_desc.elementSize=16;
  result_desc.usage=rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource;
  result_desc.defaultState=rhi::ResourceState::UnorderedAccess;
  Slang::ComPtr<rhi::IBuffer> result;checked(r.device->createBuffer(result_desc,nullptr,result.writeRef()),"filter result buffer");
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Voxel/AtlasFiltering.slang","atlas_filtering",pipeline),
      "production filtering helper pipeline");
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"filter commands");
  auto* compute=commands->beginComputePass();require(compute!=nullptr,"filter compute pass");
  auto* root=compute->bindPipeline(pipeline);require(root!=nullptr,"filter pipeline binding");
  require(bind_world_atlas(r.atlas,root),"production filter sampler bindings");
  const rhi::ShaderCursor cursor(root);
  auto calibration_desc=linear;calibration_desc.maxAnisotropy=1;
  Slang::ComPtr<rhi::ISampler> calibration;
  checked(r.device->createSampler(calibration_desc,calibration.writeRef()),"non-anisotropic calibration sampler");
  checked(cursor["calibrationSampler"].setBinding(rhi::Binding(calibration)),"calibration sampler binding");
  checked(cursor["atlasAlbedo"].setBinding(rhi::Binding(view)),"filter test texture binding");
  checked(cursor["filterInputs"].setBinding(rhi::Binding(input)),"filter inputs binding");
  checked(cursor["filterOutputs"].setBinding(rhi::Binding(result)),"filter outputs binding");
  compute->dispatchCompute(static_cast<unsigned>(inputs.size()),1,1);compute->end();
  auto submission=commands->finish();require(submission!=nullptr,"filter finish");
  checked(r.queue->submit(submission),"filter submit");checked(r.queue->waitOnHost(),"filter wait");
  checked(r.device->readBuffer(result,0,result_desc.size,output.data()),"filter output readback");
  for(const auto& sample:output)for(float value:sample)require(std::isfinite(value),"nonfinite filtered sample");
  require(output[0][0]==0 && output[1][0]==1,"magnified atlas pixels lost exact point colors");
  for(std::size_t i=2;i<ramp_begin;++i)require(std::abs(output[i][0]-.5f)<.002f,"minified checker aliases");
  float largest_step=0;
  for(std::size_t i=ramp_begin;i<plain_begin;++i) {
    const auto step_index=i-ramp_begin;const float expected=float(step_index)/80;
    // Anisotropic filtering is implementation-dependent. RX9070XT compresses
    // each trilinear transition into fractional LOD [.25,.75]. Require its
    // correct integer endpoints, interval and continuity, not invented tap weights.
    if(step_index%16==0)require(std::abs(output[i][0]-expected)<.002f,"anisotropic mip endpoint is biased");
    const float low=std::floor(float(step_index)/16)/5,high=std::ceil(float(step_index)/16)/5;
    require(output[i][0]>=low-.002f && output[i][0]<=high+.002f,"anisotropic mip ramp escaped adjacent levels");
    if(i>ramp_begin) {
      const float step=output[i][0]-output[i-1][0];
      require(step>=-.002f && step<.04f,"mip transition is discontinuous");largest_step=std::max(largest_step,step);
    }
  }
  float plain_error=0;
  for(std::size_t i=plain_begin;i<bias_begin;++i) {
    const float expected=float(i-plain_begin)/80,error=std::abs(output[i][0]-expected);
    plain_error=std::max(plain_error,error);
    if(error>.003f)std::printf("atlas_plain_mip_difference step=%zu expected=%g actual=%g\n",i-plain_begin,expected,output[i][0]);
    require(error<.003f,"non-anisotropic trilinear ramp differs from known mip interpolation");
  }
  for(unsigned mip=1;mip<=5;++mip)
    require(float(mip)/5-output[bias_begin+mip-1][0]>.1f,"negative-control derivative bias was not detected");
  std::printf("atlas_mip_ramps anisotropic_step_max=%g plain_error_max=%g old_bias_rejected=5\n",largest_step,plain_error);
  require(output[transition_begin][0]==0 && std::abs(output[sprite_begin-1][0]-.5f)<.035f,
      "magnification-to-minification transition lost point or filtered endpoint");
  for(std::size_t i=transition_begin+1;i<sprite_begin;++i) {
    const float step=output[i][0]-output[i-1][0];
    require(step>=-.002f && step<.09f,"pixel-art filtering transition pops or reverses");
  }
  require(output[sprite_begin][0]<.002f && output[sprite_begin+1][0]>.998f,"sprite minification wraps opposite edges");
  require(std::abs(output[point_begin][0]-.4f)<.002f,"packed material mip uses negative bias or interpolation");
  for(std::size_t i=oblique_begin;i<alpha_begin;++i)
    require(std::abs(output[i][0]-.5f)<.035f,"anisotropic checker aliases at grazing footprint");
  for(unsigned mode=0;mode<2;++mode)for(unsigned mip=0;mip<=4;++mip) {
    unsigned covered=0;
    for(unsigned x=0;x<alpha_samples;++x)
      covered+=output[alpha_begin+(mode*5+mip)*alpha_samples+x][3]>=.35f;
    const float coverage=float(covered)/alpha_samples,size=float(32>>mip);
    // Analytic bilinear half-step coverage at cutoff .35. The sprite coordinate
    // retains its 31/32 inset scale; anisotropic LOD precision is bounded here.
    const float expected=.5f+(mode?(.15f*32/31):.3f)/size;
    std::printf("atlas_alpha_filter mode=%s mip=%u coverage=%.6f expected=%.6f\n",
        mode?"sprite_clamp":"terrain_wrap",mip,coverage,expected);
    require(std::abs(coverage-expected)<.02f,"filtered cutout coverage collapsed or grew beyond half-step oracle");
  }
  require(r.debug.errors.load()==0,"atlas filtering validation errors");
  std::printf("atlas_filtering=passed samples=%zu mip_step_max=%g anisotropy=8 magnification=point sprite_edges=clamp\n",
      output.size(),largest_step);
}
}
