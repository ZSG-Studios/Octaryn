#include "WorldRendererInternal.h"
#include "AssetPath.h"
#include <RmlUi/Core.h>
#include <limits>
#include <string>

namespace octaryn::client::rendering {
namespace {
struct BootDocument {
  Rml::SystemInterface system;
  bool initialized{};
  ~BootDocument() {
    if(initialized)Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
    Rml::SetSystemInterface(nullptr);
  }
};

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
// The loading document is host diagnostics; product UI starts after handoff.
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
  if((!active || active->width!=config.width || active->height!=config.height ||
      active->format!=config.format || active->usage!=config.usage ||
      active->desiredImageCount!=config.desiredImageCount || active->vsync!=config.vsync) &&
      !world_rhi_ok(r.surface->configure(config)))return false;
  Slang::ComPtr<rhi::ITexture> image;
  if(!world_rhi_ok(r.surface->acquireNextImage(image.writeRef())))return false;
  if(!image)return true;
  rhi::TextureDesc desc{};
  desc.size={config.width,config.height,1};desc.format=r.color_format;
  desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::CopySource|rhi::TextureUsage::CopyDestination;
  desc.defaultState=rhi::ResourceState::RenderTarget;desc.label="startup_loading";
  Slang::ComPtr<rhi::ITexture> color;
  Slang::ComPtr<rhi::ITextureView> view;
  if(!world_rhi_ok(r.device->createTexture(desc,nullptr,color.writeRef())) ||
      !world_rhi_ok(color->getDefaultView(view.writeRef())))return false;
  BootDocument ui;
  Rml::SetSystemInterface(&ui.system);
  Rml::SetRenderInterface(rml_render_interface(r.ui_renderer));
  ui.initialized=Rml::Initialise();
  if(!ui.initialized)return false;
  char font[4096]{};
  if(!bundle_path_build(font,sizeof(font),"Assets/Ui/Fonts/Silkscreen-Regular.ttf") ||
      !Rml::LoadFontFace(font))return false;
  auto* context=Rml::CreateContext("startup",{width,height});
  if(!context)return false;
  auto* document=context->LoadDocumentFromMemory(
      "<rml><head><style>body{width:100%;height:100%;margin:0;font-family:Silkscreen;color:#dce7f5;font-size:20px;}"
      "#panel{position:absolute;left:10%;top:38%;width:80%;}"
      "h1{display:block;font-size:32px;margin-bottom:20px;}p{display:block;margin-top:14px;}"
      "#detail{color:#a4b2c6;font-size:16px;}</style></head><body><div id='panel'>"
      "<h1>Starting Octaryn</h1><p id='stage'></p><p id='detail'>"
      "Preparing graphics. You can move, resize or close this window.</p></div></body></rml>");
  if(!document)return false;
  document->GetElementById("stage")->SetInnerRML(stage);
  document->Show();
  if(!context->Update())return false;
  auto commands=r.queue->createCommandEncoder();
  if(!commands)return false;
  float background[4]{0.018f,0.025f,0.04f,1.f};
  commands->clearTextureFloat(color,{0,1,0,1},background);
  if(!render_rml(r.ui_renderer,commands,view,context,width,height))return false;
  const rhi::SubresourceRange range{0,1,0,1};
  commands->copyTexture(image,range,{},color,range,{},desc.size);
  commands->setTextureState(image,rhi::ResourceState::Present);
  auto submission=commands->finish();
  if(!submission || !r.frame_queue.submit(r.queue,submission,0))return false;
  const bool presented=world_rhi_ok(r.surface->present());
  // Release the temporary Rml buffers only after their one frame completes.
  if(!r.frame_queue.wait(0,frame_fence_timeout_ms()) || !presented)return false;
  return capture_boot_frame(r,color,stage,width,height);
}
}
