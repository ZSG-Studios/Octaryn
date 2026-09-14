#include "Probe.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace mesh_probe {
namespace {
using Pixel=std::array<float,4>;
constexpr unsigned Size=16,Count=Size*Size,Center=8*Size+8;
struct Image {Slang::ComPtr<rhi::ITexture> texture;Slang::ComPtr<rhi::ITextureView> view;};
Image texture(WorldRenderer& r,const std::vector<Pixel>& pixels,bool scalar=false) {
  rhi::TextureDesc desc{};desc.size={Size,Size,1};desc.format=scalar?rhi::Format::R32Float:rhi::Format::RGBA32Float;
  desc.usage=rhi::TextureUsage::ShaderResource|rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::CopySource;
  desc.defaultState=rhi::ResourceState::ShaderResource;
  std::vector<float> single;if(scalar)for(const auto& p:pixels)single.push_back(p[0]);
  const unsigned stride=scalar?4u:16u;
  rhi::SubresourceData data{scalar?static_cast<const void*>(single.data()):static_cast<const void*>(pixels.data()),Size*stride,Count*stride};
  Image out;checked(r.device->createTexture(desc,&data,out.texture.writeRef()),"temporal fixture texture");
  checked(out.texture->getDefaultView(out.view.writeRef()),"temporal fixture view");return out;
}
std::vector<Pixel> run(WorldRenderer& r,rhi::IComputePipeline* pipeline,bool sun,const std::vector<Pixel>& raw,
    const Pixel& previous,bool disocclusion=false,bool valid=true) {
  std::vector<Pixel> positions(Count),oldPositions(Count),zero(Count),colors(Count,Pixel{1,1,1,1}),materials(Count,Pixel{.5f,0,0,0});
  for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x) {
    positions[y*Size+x]={2*(float(x)+.5f)/Size-1,1-2*(float(y)+.5f)/Size,1,1};
    oldPositions[y*Size+x]=positions[y*Size+x];if(disocclusion)oldPositions[y*Size+x][2]+=1;
  }
  const Pixel history=sun?Pixel{previous[0],32,previous[0],0}:Pixel{previous[0],previous[1],previous[2],16};
  std::map<std::string,Image> images;
  images.emplace("positions",texture(r,positions));images.emplace("voxels",texture(r,zero));
  images.emplace("previousPositions",texture(r,oldPositions));images.emplace("previousVoxels",texture(r,zero));
  images.emplace("positionHistory",texture(r,zero));images.emplace("voxelHistory",texture(r,zero));
  images.emplace(sun?"currentShadow":"rawLighting",texture(r,raw,sun));
  images.emplace(sun?"previousShadow":"previousRadiance",texture(r,std::vector<Pixel>(Count,history)));
  const auto destination=sun?"shadowHistory":"radianceHistory";
  images.emplace(destination,texture(r,zero));
  if(sun)images.emplace("outputShadow",texture(r,zero,true));
  else {
    images.emplace("colors",texture(r,colors));images.emplace("materials",texture(r,materials));
    images.emplace("previousFast",texture(r,std::vector<Pixel>(Count,previous)));
    images.emplace("fastHistory",texture(r,zero));
  }
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"temporal fixture commands");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"temporal fixture pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"temporal fixture pipeline");rhi::ShaderCursor c(root);
  for(auto& [name,image]:images)if(c[name.c_str()].isValid())checked(c[name.c_str()].setBinding(image.view),"temporal fixture resource binding");
  const float options[4]={float(Size),float(Size),valid?1.f:0.f,.97f};checked(c["options"].setData(options,16),"temporal fixture options");
  const float view[6][4]={{0,0,0,0},{0,0,0,0},{1,0,0,0},{0,1,0,0},{0,0,1,0},{1,1,0,0}};
  const char* names[]={"eye","previousEye","previousRight","previousUp","previousForward","previousProjection"};
  for(unsigned i=0;i<6;++i)checked(c[names[i]].setData(view[i],16),"temporal fixture projection");
  pass->dispatchCompute(2,2,1);pass->end();auto submission=commands->finish();require(submission!=nullptr,"temporal fixture finish");
  checked(r.queue->submit(submission),"temporal fixture submit");checked(r.queue->waitOnHost(),"temporal fixture completion");
  Slang::ComPtr<ISlangBlob> data;rhi::SubresourceLayout layout{};
  checked(r.device->readTexture(images.at(destination).texture,0,0,data.writeRef(),&layout),"temporal fixture readback");
  require(data && layout.colPitch==sizeof(Pixel) && layout.rowPitch>=Size*sizeof(Pixel),"temporal fixture layout");
  std::vector<Pixel> output(Count);
  for(unsigned y=0;y<Size;++y)std::memcpy(output.data()+y*Size,static_cast<const char*>(data->getBufferPointer())+y*layout.rowPitch,Size*sizeof(Pixel));
  for(const auto& p:output)for(float value:p)require(std::isfinite(value),"temporal shader returned non-finite value");
  return output;
}
void cases(WorldRenderer& r,bool sun) {
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const char* shader=sun?"octaryn-client/Shaders/Shadows/Temporal.slang":"octaryn-client/Shaders/Lighting/DenoiseTemporal.slang";
  require(create_rhi_compute_pipeline(r.device,shader,"main",pipeline),"production temporal fixture shader");
  std::vector<Pixel> dark(Count,Pixel{0,0,0,1}),bright(Count,Pixel{1,1,1,1});
  // One opposite-valued neighbor retains the whole [0,1] min/max interval.
  dark[Center+1]={1,1,1,1};bright[Center+1]={0,0,0,1};
  auto output=run(r,pipeline,sun,dark,{1,1,1,1});
  require(output[Center][0]<.1f,"moving shadow leaves stale bright history inside current min/max range");
  output=run(r,pipeline,sun,bright,{0,0,0,1});
  require(output[Center][0]>.9f,"moving light leaves stale dark history inside current min/max range");
  std::vector<Pixel> noise(Count);
  for(unsigned y=0;y<Size;++y)for(unsigned x=0;x<Size;++x) {
    float value=((x+y)&1)?.2f:.8f;noise[y*Size+x]={value,value,value,1};
  }
  output=run(r,pipeline,sun,noise,{.5f,.5f,.5f,1});
  double error=0;for(unsigned y=2;y<Size-2;++y)for(unsigned x=2;x<Size-2;++x)error+=std::pow(output[y*Size+x][0]-.5f,2);
  error/=144;require(error<.002,"adaptive response discarded stable stochastic history");
  output=run(r,pipeline,sun,noise,{.1f,.1f,.1f,1},true);
  require(std::abs(output[Center][0]-.8f)<.001f,"disoccluded receiver reused stale lighting");
  output=run(r,pipeline,sun,noise,{.1f,.1f,.1f,1},false,false);
  require(std::abs(output[Center][0]-.8f)<.001f,"explicit history reset ignored");
  if(!sun) {
    std::vector<Pixel> green(Count,Pixel{0,.297f,0,1});green[Center+1]={1,0,0,1};
    output=run(r,pipeline,false,green,{1,0,0,1});
    require(output[Center][0]<.1f && output[Center][1]>.26f,"equal-luminance colored light change retained wrong hue");
  }
  std::printf("lighting_temporal_antialag=passed pass=%s moving_dark=1 moving_bright=1 stochastic_mse=%.6f disocclusion=1 reset=1 color=%u\n",
      sun?"sun":"local",error,sun?0u:1u);
}
}
void lighting_temporal_cases(Fixture& f) {
  cases(f.renderer,false);cases(f.renderer,true);
  require(f.renderer.debug.errors.load()==0,"temporal antialag GPU validation errors");
}
}
