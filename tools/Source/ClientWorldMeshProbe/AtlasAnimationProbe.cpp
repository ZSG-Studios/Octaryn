#include "Probe.h"
#include "AtlasInternal.h"
#include <cstring>

namespace mesh_probe {
void atlas_animation_cases(Fixture& fixture) {
  auto& r=fixture.renderer;
  auto& atlas=*r.atlas;
  const auto original=atlas.textures[0];
  auto desc=original->getDesc();
  desc.usage|=rhi::TextureUsage::CopySource;
  Slang::ComPtr<rhi::ITexture> test;
  checked(r.device->createTexture(desc,nullptr,test.writeRef()),"animation test texture");
  atlas.textures[0]=test;
  const auto upload=[&](double seconds,bool repeated=false) {
    require(r.frame_queue.wait(0,2000),"animation upload frame-slot wait");
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"animation upload encoder");
    require(update_world_atlas(&atlas,commands,seconds),"animated atlas recording");
    if(repeated)require(update_world_atlas(&atlas,commands,seconds),"unchanged animation tick");
    auto submission=commands->finish();require(bool(submission),"animation upload finish");
    require(r.frame_queue.submit(r.queue,submission,0),"animation upload submit");
    commit_world_atlas(&atlas);
    require(r.frame_queue.wait(0,2000),"animation upload completion");
  };
  {
    auto abandoned=r.queue->createCommandEncoder();require(bool(abandoned),"abandoned animation encoder");
    require(update_world_atlas(&atlas,abandoned,.025),"abandoned animation recording");
    for(const auto& animation:atlas.animations)require(animation.active==~0u,"unsubmitted animation committed");
  }
  unsigned frames=0,mips=0;
  std::size_t cache_bytes=0;
  for(auto& animation:atlas.animations) {
    require(animation.frames.size()==animation.ticks.size(),"animation cache frame count");
    unsigned tick=0;
    for(unsigned frame=0;frame<animation.frames.size();++frame) {
      const auto expected=atlas_layer_pixels(atlas.animation.get(),animation.first+frame,ATLAS_MIP_ALBEDO);
      require(animation.frames[frame]==expected,"animation cache changed mip bytes");
      cache_bytes+=expected.size();
      // Use the midpoint of the authored tick to avoid host floating-point ties.
      upload((tick+.5)/20.0);
      require(animation.active==frame,"animation frame timing");
      upload((tick+.5)/20.0,true);
      require(animation.active==frame,"unchanged animation changed the active frame");
      std::size_t offset=0;
      for(unsigned mip=0,size=32;mip<6;++mip,size/=2) {
        Slang::ComPtr<ISlangBlob> bytes;rhi::SubresourceLayout layout{};
        checked(r.device->readTexture(test,animation.layer,mip,bytes.writeRef(),&layout),"animated mip readback");
        require(bytes && layout.colPitch==4 && layout.rowPitch>=size*4 &&
            bytes->getBufferSize()>=layout.rowPitch*size,"animated mip readback layout");
        for(unsigned y=0;y<size;++y)
          require(std::memcmp(static_cast<const unsigned char*>(bytes->getBufferPointer())+y*layout.rowPitch,
              expected.data()+offset+y*size*4,size*4)==0,"animated GPU mip differs from original generator");
        offset+=size*size*4;++mips;
      }
      tick+=animation.ticks[frame];++frames;
    }
    upload((animation.total+.5)/20.0);
    require(animation.active==0,"animation did not wrap to first frame");
  }
  atlas.textures[0]=original;
  for(auto& animation:atlas.animations)animation.active=animation.pending=~0u;
  require(r.debug.errors.load()==0,"animation Vulkan/RHI validation errors");
  std::printf("atlas_animation=passed frames=%u gpu_mips=%u cache_bytes=%zu exact_bytes=1 timing=1 wrap=1\n",frames,mips,cache_bytes);
}
}
