#include "WorldRendererInternal.h"
#include <limits>
#include <string>

namespace octaryn::client::rendering {
namespace {
bool capture_boot_frame(WorldRenderer& r,rhi::ITexture* color,const char* stage,int width,int height) {
  if(r.boot_captured)return true;
  const auto* path=SDL_getenv("OCTARYN_CLIENT_BOOT_CAPTURE_PATH");
  if(!path || !*path)return true;
  Slang::ComPtr<ISlangBlob> pixels;
  rhi::SubresourceLayout layout{};
  if(!world_rhi_ok(r.device->readTexture(color,0,0,pixels.writeRef(),&layout)) || !pixels ||
      layout.colPitch!=4 || layout.rowPitch<static_cast<rhi::Size>(width)*4 ||
      layout.rowPitch>static_cast<rhi::Size>(std::numeric_limits<int>::max()) ||
      pixels->getBufferSize()/layout.rowPitch<static_cast<rhi::Size>(height)) {
    std::fprintf(stderr,"client_boot_capture failed=readback path=%s\n",path);
    return false;
  }
  const auto format=color->getDesc().format;
  const bool bgra=format==rhi::Format::BGRA8Unorm || format==rhi::Format::BGRA8UnormSrgb;
  SDL_Surface* surface=SDL_CreateSurfaceFrom(width,height,bgra?SDL_PIXELFORMAT_BGRA32:SDL_PIXELFORMAT_RGBA32,
      const_cast<void*>(pixels->getBufferPointer()),static_cast<int>(layout.rowPitch));
  const bool saved=surface && SDL_SaveBMP(surface,path);
  SDL_DestroySurface(surface);
  if(!saved) {
    std::fprintf(stderr,"client_boot_capture failed=save path=%s error=%s\n",path,SDL_GetError());
    return false;
  }
  r.boot_captured=true;
  std::printf("client_boot_capture stage=%s size=%dx%d path=%s\n",stage,width,height,path);
  return true;
}
}

// Called only while initialization is suspended at its main-thread handoff.
// Product documents and fonts are loaded by the selected game after handoff.
bool world_renderer_boot_frame(WorldRenderer& r,const char* stage) {
  int width{},height{};
  SDL_GetWindowSizeInPixels(r.window,&width,&height);
  if(width<=0 || height<=0 || (SDL_GetWindowFlags(r.window)&SDL_WINDOW_MINIMIZED))return true;
  if(!r.frame_queue.synchronize(r.queue,frame_fence_timeout_ms()))return false;
  rhi::SurfaceConfig config{};
  config.format=r.color_format;
  config.usage=rhi::TextureUsage::CopyDestination;
  config.width=static_cast<std::uint32_t>(width);
  config.height=static_cast<std::uint32_t>(height);
  config.desiredImageCount=2;
  config.vsync=true;
  r.present_dirty=true;
  const auto* active=r.surface->getConfig();
  if(!r.hidden_offscreen && (!active || active->width!=config.width || active->height!=config.height ||
      active->format!=config.format || active->usage!=config.usage ||
      active->desiredImageCount!=config.desiredImageCount || active->vsync!=config.vsync) &&
      !world_rhi_ok(r.surface->configure(config)))return false;
  Slang::ComPtr<rhi::ITexture> image;
  if(!r.hidden_offscreen && !world_rhi_ok(r.surface->acquireNextImage(image.writeRef())))return false;
  if(!r.hidden_offscreen && !image)return true;
  rhi::TextureDesc desc{};
  desc.size={config.width,config.height,1};desc.format=r.color_format;
  desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::CopySource|rhi::TextureUsage::CopyDestination|rhi::TextureUsage::ShaderResource;
  desc.defaultState=rhi::ResourceState::RenderTarget;desc.label="startup_loading";
  Slang::ComPtr<rhi::ITexture> color;
  if(!world_rhi_ok(r.device->createTexture(desc,nullptr,color.writeRef())))return false;
  auto commands=r.queue->createCommandEncoder();
  if(!commands)return false;
  float background[4]{0.f,0.f,0.f,1.f};
  commands->clearTextureFloat(color,{0,1,0,1},background);
  const rhi::SubresourceRange range{0,1,0,1};
  if(image) {
    commands->copyTexture(image,range,{},color,range,{},desc.size);
    commands->setTextureState(image,rhi::ResourceState::Present);
  }
  auto submission=commands->finish();
  if(!submission || !r.frame_queue.submit(r.queue,submission,0))return false;
  const bool presented=!image || world_rhi_ok(r.surface->present());
  // Complete this startup frame before returning control to initialization.
  if(!r.frame_queue.wait(0,frame_fence_timeout_ms()) || !presented)return false;
  return capture_boot_frame(r,color,stage,width,height);
}
}
