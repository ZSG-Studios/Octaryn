#include "Probe.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>

namespace mesh_probe {
namespace {
using Pixel=std::array<float,4>;
struct Volume {
  Slang::ComPtr<rhi::IBuffer> controls,probes,irradiance,distance;
  float spacing{};
};
Volume volume(WorldRenderer& r,float spacing,float energy,unsigned state) {
  constexpr unsigned Count=512,Resolution=2;
  std::vector<DDGIControl> controls(Count);
  std::vector<DDGIProbe> probes(Count);
  for(int z=-4;z<4;++z)for(int y=-4;y<4;++y)for(int x=-4;x<4;++x) {
    const unsigned index=unsigned((x+8)%8+8*((y+8)%8+8*((z+8)%8)));
    controls[index].cell={x,y,z};controls[index].version=1;
    probes[index].metadata[0]=1;probes[index].metadata[1]=1;
    probes[index].metadata[3]=state==0?16:0;
    probes[index].offset[3]=state==2?1.f:0.f;
  }
  std::vector<Pixel> irradiance(Count*Resolution*Resolution,Pixel{energy,energy,energy,1});
  std::vector<std::array<float,2>> distance(irradiance.size(),std::array<float,2>{64,4096});
  Volume out;out.spacing=spacing;
  out.controls=buffer(r,controls.data(),controls.size()*sizeof(DDGIControl),sizeof(DDGIControl),rhi::BufferUsage::ShaderResource);
  out.probes=buffer(r,probes.data(),probes.size()*sizeof(DDGIProbe),sizeof(DDGIProbe),rhi::BufferUsage::ShaderResource);
  out.irradiance=buffer(r,irradiance.data(),irradiance.size()*sizeof(Pixel),sizeof(Pixel),rhi::BufferUsage::ShaderResource);
  out.distance=buffer(r,distance.data(),distance.size()*8,8,rhi::BufferUsage::ShaderResource);
  return out;
}
void bind(rhi::IShaderObject* root,const Volume& v,bool fine) {
  const std::string prefix=fine?"ddgiFine":"ddgi";rhi::ShaderCursor c(root);
  const auto resource=[&](const char* suffix,rhi::IBuffer* value) {
    checked(c[(prefix+suffix).c_str()].setBinding(rhi::Binding(value)),"DDGI sample fixture buffer binding");
  };
  resource("Controls",v.controls);resource("Probes",v.probes);
  resource("Irradiance",v.irradiance);resource("Distance",v.distance);
  const unsigned grid[4]={8,8,8,1},frame[4]={16,112,2,2};
  const int origin[4]={-4,-4,-4,0};
  const float fade[4]={-3,-3,-3,0},parameters[4]={v.spacing,.94f,64,.05f};
  checked(c[(prefix+"Grid").c_str()].setData(grid,16),"DDGI fixture grid");
  checked(c[(prefix+"Origin").c_str()].setData(origin,16),"DDGI fixture origin");
  checked(c[(prefix+"FadeOrigin").c_str()].setData(fade,16),"DDGI fixture fade");
  checked(c[(prefix+"Parameters").c_str()].setData(parameters,16),"DDGI fixture parameters");
  checked(c[(prefix+"Frame").c_str()].setData(frame,16),"DDGI fixture frame");
}
std::array<Pixel,4> run(WorldRenderer& r,rhi::IComputePipeline* pipeline,const Volume& a,const Volume& b) {
  std::array<Pixel,4> output{};
  auto result=buffer(r,output.data(),sizeof(output),sizeof(Pixel),rhi::BufferUsage::UnorderedAccess);
  auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"DDGI fixture commands");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"DDGI fixture compute pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"DDGI fixture pipeline bind");
  bind(root,a,false);bind(root,b,true);
  checked(rhi::ShaderCursor(root)["result"].setBinding(rhi::Binding(result)),"DDGI fixture output binding");
  pass->dispatchCompute(4,1,1);pass->end();auto submission=commands->finish();
  checked(r.queue->submit(submission),"DDGI fixture submission");checked(r.queue->waitOnHost(),"DDGI fixture completion");
  checked(r.device->readBuffer(result,0,sizeof(output),output.data()),"DDGI fixture readback");
  for(const auto& pixel:output)for(float value:pixel)require(std::isfinite(value),"DDGI sampling returned non-finite value");
  return output;
}
}
void ddgi_volume_cases(Fixture& f) {
  auto& r=f.renderer;Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Voxel/DDGIVolume.slang","main",pipeline),
      "production DDGI sample fixture shader");
  const auto coarse=volume(r,4,8,0),fine=volume(r,1,2,0);
  const auto primary=run(r,pipeline,coarse,fine),recursive=run(r,pipeline,fine,coarse);
  for(unsigned i=0;i<4;++i)for(unsigned channel=0;channel<4;++channel)
    require(std::abs(primary[i][channel]-recursive[i][channel])<1e-5f,"recursive and primary DDGI disagree on cascade order");
  require(std::abs(primary[0][0]-2)<1e-4f && std::abs(primary[1][0]-5)<1e-4f &&
      std::abs(primary[2][0]-8)<1e-4f,"DDGI dense transition does not preserve energy");
  require(std::abs(primary[3][0]-primary[1][0])<.001f,"DDGI dense boundary is discontinuous");
  const auto pending=volume(r,1,0,1),solid=volume(r,1,0,2),dark=volume(r,1,0,0);
  const auto unknown=run(r,pipeline,coarse,pending),blocked=run(r,pipeline,coarse,solid),unlit=run(r,pipeline,coarse,dark);
  require(std::abs(unknown[0][0]-8)<1e-4f,"uninitialized fine volume blacked out valid coarse history");
  require(blocked[0][0]==0 && blocked[0][3]==1,"confirmed embedded probes leaked coarse illumination");
  require(unlit[0][0]==0 && unlit[0][3]==1,"valid dark fine field was mistaken for missing data");
  require(r.debug.errors.load()==0,"DDGI field sample graphics validation errors");
  std::puts("ddgi_volume_sampling=passed swapped_recursive_order=1 constant_energy=1 continuous_boundary=1 pending_handoff=1 solid_occlusion=1 dark_field=1");
}
}
