#include "Probe.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>

namespace mesh_probe {
namespace {
using Pixel=std::array<float,4>;
std::array<Pixel,6> leak_field(WorldRenderer& r,rhi::IComputePipeline* pipeline,float spacing,unsigned mode) {
  constexpr unsigned Count=512,Texels=4;
  std::vector<DDGIControl> controls(Count);
  std::vector<DDGIProbe> probes(Count);
  std::vector<Pixel> irradiance(Count*Texels,Pixel{8,4,2,1});
  std::vector<std::array<float,2>> distance(Count*Texels,
      mode==0?std::array<float,2>{1024,1048576}:std::array<float,2>{.1f,.01f});
  for(int z=0;z<8;++z)for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
    const unsigned i=static_cast<unsigned>(x+8*(y+8*z));
    controls[i].cell={x,y,z};controls[i].version=1;
    probes[i].metadata[0]=1;probes[i].metadata[1]=99;probes[i].metadata[3]=100;
  }
  if(mode>=2)for(unsigned t=0;t<Texels;++t) {
    const unsigned i=(3+8*(3+8*3))*Texels+t;
    distance[i]={1024,1048576};
    if(mode==3)irradiance[i]={0,0,0,1};
  }
  const std::array<Pixel,6> normals={Pixel{1,0,0,0},Pixel{-1,0,0,0},Pixel{0,1,0,0},
      Pixel{0,-1,0,0},Pixel{0,0,1,0},Pixel{0,0,-1,0}};
  std::array<Pixel,6> positions,output{};
  positions.fill(Pixel{3.5f*spacing,3.5f*spacing,3.5f*spacing,0});
  auto c=buffer(r,controls.data(),controls.size()*sizeof(DDGIControl),sizeof(DDGIControl),rhi::BufferUsage::ShaderResource);
  auto p=buffer(r,probes.data(),probes.size()*sizeof(DDGIProbe),sizeof(DDGIProbe),rhi::BufferUsage::ShaderResource);
  auto e=buffer(r,irradiance.data(),irradiance.size()*sizeof(Pixel),sizeof(Pixel),rhi::BufferUsage::ShaderResource);
  auto d=buffer(r,distance.data(),distance.size()*8,8,rhi::BufferUsage::ShaderResource);
  auto x=buffer(r,positions.data(),sizeof(positions),sizeof(Pixel),rhi::BufferUsage::ShaderResource);
  auto n=buffer(r,normals.data(),sizeof(normals),sizeof(Pixel),rhi::BufferUsage::ShaderResource);
  auto result=buffer(r,output.data(),sizeof(output),sizeof(Pixel),rhi::BufferUsage::UnorderedAccess);
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"DDGI leak encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"DDGI leak compute pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"DDGI leak pipeline");
  rhi::ShaderCursor cursor(root);
  const auto bind=[&](const std::string& name,rhi::IBuffer* value) {
    checked(cursor[name.c_str()].setBinding(rhi::Binding(value)),"DDGI leak buffer binding");
  };
  for(bool fine:{false,true}) {
    const std::string prefix=fine?"ddgiFine":"ddgi";
    bind(prefix+"Controls",c);bind(prefix+"Probes",p);bind(prefix+"Irradiance",e);bind(prefix+"Distance",d);
    const unsigned grid[4]={8,8,8,fine?0u:1u},frame[4]={100,176,2,2};
    const int origin[4]={0,0,0,0};
    const float fade[4]={0,0,0,.2f},parameters[4]={spacing,.94f,1024,0};
    checked(cursor[(prefix+"Grid").c_str()].setData(grid,16),"DDGI leak grid");
    checked(cursor[(prefix+"Frame").c_str()].setData(frame,16),"DDGI leak frame");
    checked(cursor[(prefix+"Origin").c_str()].setData(origin,16),"DDGI leak origin");
    checked(cursor[(prefix+"FadeOrigin").c_str()].setData(fade,16),"DDGI leak fade");
    checked(cursor[(prefix+"Parameters").c_str()].setData(parameters,16),"DDGI leak parameters");
  }
  bind("positions",x);bind("normals",n);bind("results",result);
  pass->dispatchCompute(6,1,1);pass->end();auto submission=commands->finish();
  checked(r.queue->submit(submission),"DDGI leak submit");checked(r.queue->waitOnHost(),"DDGI leak completion");
  checked(r.device->readBuffer(result,0,sizeof(output),output.data()),"DDGI leak readback");
  return output;
}
}
void ddgi_leak_cases(Fixture& f) {
  auto& r=f.renderer;Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/DDGILeakProbe.slang","main",pipeline),
      "production DDGI leak fixture shader");
  unsigned count=0;
  for(float spacing:{1.f,4.f,8.f,16.f,64.f})for(unsigned mode=0;mode<4;++mode) {
    const auto values=leak_field(r,pipeline,spacing,mode);
    for(const auto& value:values) {
      for(float channel:value)require(std::isfinite(channel),"non-finite DDGI leak fixture output");
      require(value[3]>.999f,"observed cage lost coverage");
      if(mode==0 || mode==2)require(std::abs(value[0]-8)<1e-4f,"visible constant field lost energy");
      else require(value[0]<(mode==1?.016f:.0001f),"blocked bright cage leaked irradiance");
      ++count;
    }
  }
  require(r.debug.errors.load()==0,"DDGI leak fixture graphics validation errors");
  std::printf("ddgi_leak_gpu=passed cases=%u production_sampling=1 scene_traversal=not_exercised\n",count);
}
}
