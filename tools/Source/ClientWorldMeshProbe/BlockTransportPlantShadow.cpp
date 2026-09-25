#include "BlockTransportPlantProbe.h"

namespace mesh_probe::plant_probe {
namespace {
struct ShadowCase {Vector point,direction;unsigned plane,side;float expected;};
ShadowCase blocked(SDL_Surface* png,unsigned layer,unsigned plane,unsigned side) {
  const auto n=normal(plane,side);
  for(unsigned sy=0;sy<32;++sy)for(unsigned sx=0;sx<32;++sx) {
    if(!opaque(png,layer,sx,sy) || (sx>=14 && sx<=17))continue;
    const auto source=point({8,-24,8},plane,(float(sx)+.5f)/32,(float(sy)+.5f)/32);
    for(unsigned ty=0;ty<32;++ty)for(unsigned tx=0;tx<32;++tx) {
      if(!opaque(png,layer,tx,ty) || (tx>=14 && tx<=17))continue;
      const auto target=point({8,-24,8},1-plane,(float(tx)+.5f)/32,(float(ty)+.5f)/32);
      Vector direction{target[0]-source[0],target[1]-source[1],target[2]-source[2]};
      const float length=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
      if(length<.1f)continue;
      for(auto& component:direction)component/=length;
      if(direction[0]*n[0]+direction[2]*n[2]<.2f)continue;
      // Account for the production outward origin before accepting the CPU blocker.
      const auto other=normal(1-plane,0);Vector origin=source;
      for(unsigned i=0;i<3;++i)origin[i]+=n[i]*.002f+direction[i]*.0005f;
      const Vector center{8.5f,-23.5f,8.5f};
      const float denominator=other[0]*direction[0]+other[2]*direction[2];
      if(std::abs(denominator)<.05f)continue;
      const float distance=((center[0]-origin[0])*other[0]+(center[2]-origin[2])*other[2])/denominator;
      if(distance<.02f || distance>2)continue;
      const float u=1-(origin[0]+direction[0]*distance-8),v=1-(origin[1]+direction[1]*distance+24);
      if(u<=0 || u>=1 || v<=0 || v>=1)continue;
      const unsigned x=unsigned(std::floor(.5f+31*u)),y=unsigned(std::floor(.5f+31*v));
      if(x!=tx || y!=ty || !opaque(png,layer,x,y))continue;
      return {source,direction,plane,side,0};
    }
  }
  require(false,"BT plant PNG fixture has no crossed-plane shadow pair");return {};
}
float run(Fixture& f,const ShadowCase& test,unsigned layer,rhi::IComputePipeline* pipeline) {
  auto& r=f.renderer;const auto start=std::chrono::steady_clock::now();
  r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT plant shadow frame reuse");
  const Pixel position{test.point[0],test.point[1],test.point[2],1};
  const unsigned voxel=encoded_voxel(layer,test.plane,test.side);
  const Pixel packed{float(voxel&255)/255,float((voxel>>8)&255)/255,float((voxel>>16)&255)/255,float(voxel>>24)/255};
  Slang::ComPtr<rhi::ITexture> positions,voxels,visibility;
  Slang::ComPtr<rhi::ITextureView> position_view,voxel_view,visibility_view;
  rhi::TextureDesc desc{};desc.size={1,1,1};desc.format=rhi::Format::RGBA32Float;
  desc.usage=rhi::TextureUsage::ShaderResource;desc.defaultState=rhi::ResourceState::ShaderResource;
  rhi::SubresourceData data{position.data(),sizeof(Pixel),sizeof(Pixel)};
  checked(r.device->createTexture(desc,&data,positions.writeRef()),"BT plant shadow receiver position");
  data.data=packed.data();checked(r.device->createTexture(desc,&data,voxels.writeRef()),"BT plant shadow receiver normal");
  desc.format=rhi::Format::R32Float;desc.usage=rhi::TextureUsage::ShaderResource|
      rhi::TextureUsage::UnorderedAccess|rhi::TextureUsage::CopySource;
  checked(r.device->createTexture(desc,nullptr,visibility.writeRef()),"BT plant shadow result");
  checked(positions->getDefaultView(position_view.writeRef()),"BT plant shadow position view");
  checked(voxels->getDefaultView(voxel_view.writeRef()),"BT plant shadow normal view");
  checked(visibility->getDefaultView(visibility_view.writeRef()),"BT plant shadow result view");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT plant shadow encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT plant shadow pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT plant shadow pipeline binding");
  require(world_ray_bind(r,root) && bind_world_atlas(r.atlas,root),"BT plant shadow production scene");
  const rhi::ShaderCursor cursor(root);const Pixel eye{},sun{test.direction[0],test.direction[1],test.direction[2],1};
  const Pixel settings{1,16,.002f,0};const std::array<unsigned,2> extent{1,1};
  const std::array<float,2> sampling{};const unsigned samples=1;const float range=0;
  checked(cursor["positions"].setBinding(position_view),"BT plant shadow positions");
  checked(cursor["voxels"].setBinding(voxel_view),"BT plant shadow encoded voxel");
  checked(cursor["visibility"].setBinding(visibility_view),"BT plant shadow visibility");
  checked(cursor["eye"].setData(eye.data(),sizeof(eye)),"BT plant shadow eye");
  checked(cursor["sun"].setData(sun.data(),sizeof(sun)),"BT plant shadow sun");
  checked(cursor["extent"].setData(extent.data(),sizeof(extent)),"BT plant shadow extent");
  checked(cursor["sampling"].setData(sampling.data(),sizeof(sampling)),"BT plant shadow deterministic center ray");
  checked(cursor["shadowRange"].setData(&range,sizeof(range)),"BT plant shadow range");
  checked(cursor["shadowSamples"].setData(&samples,sizeof(samples)),"BT plant shadow sample count");
  checked(cursor["raySettings"].setData(settings.data(),sizeof(settings)),"BT plant shadow exact alpha mip");
  pass->dispatchCompute(1,1,1);pass->end();submit(r,commands);
  Slang::ComPtr<ISlangBlob> bytes;rhi::SubresourceLayout layout{};
  checked(r.device->readTexture(visibility,0,0,bytes.writeRef(),&layout),"BT plant shadow readback");
  require(bytes && layout.colPitch==4 && bytes->getBufferSize()>=4,"BT plant shadow readback layout");
  float result;std::memcpy(&result,bytes->getBufferPointer(),sizeof(result));cap(r,start);return result;
}
}
void shadows(Fixture& f,SDL_Surface* png,unsigned layer) {
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(block_transport_pipeline(f.renderer.device,"octaryn-client/Shaders/RayTracing/Shadow.slang",
      "main",pipeline),"BT plant production primary shadow pipeline");
  unsigned checks=0;
  for(unsigned plane=0;plane<2;++plane)for(unsigned side=0;side<2;++side) {
    const auto occluded=blocked(png,layer,plane,side);
    auto clear=occluded;clear.direction=normal(plane,side);clear.expected=1;
    auto back_sun=occluded;back_sun.side=1-side;back_sun.expected=1;
    for(const auto& test:{occluded,clear,back_sun}) {
      require(std::abs(run(f,test,layer,pipeline)-test.expected)<1e-6f,
          "BT plant primary shadow lost crossed occlusion, self-hit bias or true-side sun policy");++checks;
    }
  }
  std::printf("block_transport_plant_shadows=passed production_shadow=1 png_oracle=1 crossed_plane_occlusion=1 outward_bias=1 no_sun_facing_flip=1 cases=%u\n",checks);
}
}
