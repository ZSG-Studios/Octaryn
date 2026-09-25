#include "WorldRendererInternal.h"
#include "FrameWatchdog.h"
#include "TemporalCapture.h"
#include "TemporalObservation.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cstdlib>
namespace octaryn::client::rendering {
bool capture_lighting(WorldRenderer&,const char*);
namespace {
bool open_world_renderer_capture_ui(WorldRenderer* renderer,const char* path) {
  if(!renderer)return false;
  WorldRenderer& r=*renderer;
  auto* context=r.ui_context;
  if(!context || !r.ui_renderer || !path || !*path)return false;
  const auto window_dimensions=context->GetDimensions();
  int width=window_dimensions.x,height=window_dimensions.y;
  // Expand to the full document content so panels stretching past the window
  // are captured whole instead of clipped at the viewport edge. Scrollable
  // modals are unclamped for the capture so their content flows into the
  // document size as well.
  auto* document=context->GetDocument(0);
  Rml::ElementList panels;
  if(document) {
    document->GetElementsByClassName(panels,"panel");
    for(auto* element:panels) {
      element->SetProperty("max-height","none");
      element->SetProperty("overflow-y","visible");
    }
  }
  context->Update();
  if(document) {
    width=std::max(width,static_cast<int>(document->GetScrollWidth()));
    height=std::max(height,static_cast<int>(document->GetScrollHeight()));
  }
  width=std::clamp(width,1,8192);height=std::clamp(height,1,8192);
  context->SetDimensions({width,height});
  context->Update();
  rhi::TextureDesc desc{};desc.size={static_cast<std::uint32_t>(width),static_cast<std::uint32_t>(height),1};
  desc.format=rhi::Format::RGBA8Unorm;
  desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::CopySource;
  desc.defaultState=rhi::ResourceState::RenderTarget;
  Slang::ComPtr<rhi::ITexture> texture;Slang::ComPtr<rhi::ITextureView> view;
  bool ok=world_rhi_ok(r.device->createTexture(desc,nullptr,texture.writeRef())) &&
    world_rhi_ok(texture->getDefaultView(view.writeRef()));
  auto commands=ok?r.queue->createCommandEncoder():nullptr;
  if(commands) {
    float clear[4]{0,0,0,0};
    commands->clearTextureFloat(texture,{0,1,0,1},clear);
    ok=render_rml(r.ui_renderer,commands,view,context,width,height);
    auto submission=commands->finish();
    ok=ok && submission && world_rhi_ok(r.queue->submit(submission)) &&
        r.frame_queue.synchronize(r.queue,frame_fence_timeout_ms());
  }
  for(auto* element:panels) {
    element->RemoveProperty("max-height");
    element->RemoveProperty("overflow-y");
  }
  context->SetDimensions(window_dimensions);
  context->Update();
  if(!ok)return false;
  Slang::ComPtr<ISlangBlob> pixels;rhi::SubresourceLayout layout{};
  if(SLANG_FAILED(r.device->readTexture(texture,0,0,pixels.writeRef(),&layout)) || !pixels || layout.colPitch!=4 ||
     pixels->getBufferSize()<layout.rowPitch*static_cast<rhi::Size>(height))return false;
  SDL_Surface* surface=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA32,
      const_cast<void*>(pixels->getBufferPointer()),static_cast<int>(layout.rowPitch));
  if(!surface)return false;
  const bool saved=SDL_SaveBMP(surface,path);
  SDL_DestroySurface(surface);
  if(saved)std::printf("ui_capture path=%s size=%dx%d\n",path,width,height);
  return saved;
}
bool world_renderer_capture(WorldRenderer& r,const WorldCamera& camera) {
  const char* path=SDL_GetEnvironmentVariable(SDL_GetEnvironment(),"OCTARYN_CLIENT_CAPTURE_PATH");
  const auto* requested=SDL_getenv("OCTARYN_CLIENT_CAPTURE_COUNT");
  const unsigned captures=requested?unsigned(std::clamp(std::atoi(requested),1,64)):1;
  const auto* stride=SDL_getenv("OCTARYN_CLIENT_CAPTURE_STRIDE");
  const unsigned interval=stride?unsigned(std::clamp(std::atoi(stride),1,120)):16;
  const auto* first_frame=SDL_getenv("OCTARYN_CLIENT_CAPTURE_MIN_FRAME");
  const unsigned minimum_frame=first_frame?unsigned(std::clamp(std::atoi(first_frame),120,10000)):120;
  // A loaded GLB map is the capture readiness gate.
  const bool world_resident=r.map!=nullptr;
  if (!path || !*path || !r.capture_enabled || r.capture_count>=captures || r.frames<minimum_frame ||
      (r.capture_count && r.frames-r.capture_last_frame<interval) ||
      !world_resident) return true;
  if(r.ray_enabled && world_ray_available(r)) {
    const auto ray=world_ray_stats(r);
    if(ray.pending_columns || ray.active_jobs)return true;
  }
  if(r.local_lighting.active || (r.ray_enabled && world_ray_available(r))) {
    if(r.capture_scene_revision!=r.scene_changes.revision()) {
      r.capture_scene_revision=r.scene_changes.revision();r.capture_stable_frame=r.frames;
    }
    unsigned stable_frames=64;
    // Diagnostic edit sequences retain initial convergence but capture the
    // immediate response after later scene revisions, instead of hiding it.
    if(r.capture_count)if(const auto* wait=SDL_getenv("OCTARYN_CLIENT_CAPTURE_STABLE_FRAMES"))
      stable_frames=unsigned(std::clamp(std::atoi(wait),0,120));
    if(r.frames-r.capture_stable_frame<stable_frames)return true;
  }
  if(!r.frame_queue.wait(r.active_frame,frame_fence_timeout_ms()))return false;
  TemporalObservation observation(r.temporal.last,r.temporal.mode!=0);
  std::string sample_path;
  if(r.capture_count) {sample_path=std::string(path)+".sample-"+std::to_string(r.capture_count)+".bmp";path=sample_path.c_str();}
  if(!capture_lighting(r,path))return false;
  const auto* temporal_count=SDL_getenv("OCTARYN_CLIENT_CAPTURE_TEMPORAL_COUNT");
  const unsigned temporal_captures=temporal_count?unsigned(std::clamp(std::atoi(temporal_count),1,4)):1;
  if(const auto* temporal=SDL_getenv("OCTARYN_CLIENT_CAPTURE_TEMPORAL");r.capture_count<temporal_captures && temporal && std::string_view(temporal)=="1")
    if(!capture_temporal(r.temporal,r.device,r.target().hdr.scene,r.target().depth,r.active_frame,path))return false;
  Slang::ComPtr<ISlangBlob> pixels;
  rhi::SubresourceLayout layout{};
  if (SLANG_FAILED(r.device->readTexture(r.target().color,0,0,pixels.writeRef(),&layout)) || !pixels || layout.colPitch!=4 ||
      pixels->getBufferSize()<layout.rowPitch*static_cast<rhi::Size>(r.height)) return false;
  const auto format=r.target().color->getDesc().format;
  const bool bgra=format==rhi::Format::BGRA8Unorm || format==rhi::Format::BGRA8UnormSrgb;
  SDL_Surface* surface=SDL_CreateSurfaceFrom(r.width,r.height,
      bgra?SDL_PIXELFORMAT_BGRA32:SDL_PIXELFORMAT_RGBA32,
      const_cast<void*>(pixels->getBufferPointer()),static_cast<int>(layout.rowPitch));
  if (!surface) return false;
  const bool saved=SDL_SaveBMP(surface,path);
  SDL_DestroySurface(surface);
  if (!saved) return false;
  const auto* data=static_cast<const unsigned char*>(pixels->getBufferPointer());
  std::uint64_t nonclear{};
  for (int y=0;y<r.height;++y) for (int x=0;x<r.width;++x) {
    const auto* pixel=data+static_cast<rhi::Size>(y)*layout.rowPitch+x*4;
    if (std::abs(int(pixel[bgra?2:0])-102)>1 ||
        std::abs(int(pixel[1])-163)>1 || std::abs(int(pixel[bgra?0:2])-219)>1) ++nonclear;
  }
  r.captured=true;
  if(!capture_temporal_observation(r.temporal,r.frames,r.active_frame,observation.elapsed_ms(),path))return false;
  ++r.capture_count;r.capture_last_frame=r.frames;
  std::fprintf(stdout,"world_capture frame=%llu nonclear_pixels=%llu eye=%.6f,%.6f,%.6f yaw=%.6f pitch=%.6f fov=%.6f path=%s\n",
      static_cast<unsigned long long>(r.frames),
      static_cast<unsigned long long>(nonclear),camera.x,camera.y,camera.z,
      camera.yaw,camera.pitch,camera.vertical_fov,path);
  std::fflush(stdout);
  return true;
}
}
