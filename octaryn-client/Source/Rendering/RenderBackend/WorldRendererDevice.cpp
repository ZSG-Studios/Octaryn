#include "WorldRendererInternal.h"
#include "SlangShaderPath.h"
#include "AssetPath.h"
#include "LightingSystem.h"
#include "ShaderCache.h"
#include <cstdio>
#include <cstring>
namespace octaryn::client::rendering {
namespace {
// Bindless capacity for indirect world draws; kept for map/indirect paths.
constexpr std::uint32_t WorldDescriptorCapacity=32768;
bool window_handle(SDL_Window* window,rhi::WindowHandle& handle) {
  const auto properties=SDL_GetWindowProperties(window);
#if defined(_WIN32)
  auto* native=SDL_GetPointerProperty(properties,SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr);
  if(!native) return false;
  handle=rhi::WindowHandle::fromHwnd(native);
#elif defined(__APPLE__)
  auto* native=SDL_GetPointerProperty(properties,SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,nullptr);
  if(!native) return false;
  handle=rhi::WindowHandle::fromNSWindow(native);
#else
  auto* display=SDL_GetPointerProperty(properties,SDL_PROP_WINDOW_X11_DISPLAY_POINTER,nullptr);
  auto native=SDL_GetNumberProperty(properties,SDL_PROP_WINDOW_X11_WINDOW_NUMBER,0);
  if(!display || !native) return false;
  handle=rhi::WindowHandle::fromXlibWindow(display,static_cast<std::uint32_t>(native));
#endif
  return true;
}
}
namespace {
bool configure_surface(WorldRenderer& r,unsigned width,unsigned height) {
  rhi::SurfaceConfig config{};
  config.format=r.color_format;config.usage=rhi::TextureUsage::CopyDestination;
  config.width=width;config.height=height;
  config.desiredImageCount=r.present_mode==2?3u:2u;
  config.vsync=r.present_mode==1;
  if(!world_rhi_ok(r.surface->configure(config))) {r.status="surface_configure_failed";return false;}
  r.present_dirty=false;return true;
}
bool resize_targets(WorldRenderer& r,unsigned width,unsigned height) {
  r.temporal.mode=r.temporal.requested_mode;
  if(!resize_temporal(r.temporal,r.device,width,height,r.frame_queue.count()))return false;
  for(unsigned slot=0;slot<r.frame_queue.count();++slot) {
  auto& target=r.targets[slot];
  target.initialized=false;
  target.depth_view.setNull();target.depth.setNull();target.color_view.setNull();target.color.setNull();
  rhi::TextureDesc desc{};
  desc.size={r.temporal.allocation_width,r.temporal.allocation_height,1};desc.format=rhi::Format::D32Float;
  desc.usage=rhi::TextureUsage::DepthStencil|rhi::TextureUsage::ShaderResource|rhi::TextureUsage::CopySource;desc.defaultState=rhi::ResourceState::DepthWrite;
  desc.label="world_depth";
  if(!world_rhi_ok(r.device->createTexture(desc,nullptr,target.depth.writeRef())) ||
     !world_rhi_ok(target.depth->getDefaultView(target.depth_view.writeRef()))) return false;
  desc.format=r.color_format;
  desc.size={width,height,1};
  desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::UnorderedAccess|
      rhi::TextureUsage::CopySource|rhi::TextureUsage::CopyDestination;
  desc.defaultState=rhi::ResourceState::RenderTarget;desc.label="world_color";
  if(!world_rhi_ok(r.device->createTexture(desc,nullptr,target.color.writeRef())) ||
     !world_rhi_ok(target.color->getDefaultView(target.color_view.writeRef()))) return false;
  if(!resize_world_hdr(r.device,target.hdr,r.temporal.allocation_width,r.temporal.allocation_height)) return false;
  }
  return true;
}
}
bool open_world_renderer_prepare_temporal(WorldRenderer* r) {
  if(!r || r->width<=0 || r->height<=0)return false;
  if(r->temporal.requested_mode==r->temporal.mode && !r->temporal.reconfigure)
    return prepare_rt_shadow_targets(*r) && prepare_map_reflections(*r);
  // Exclusive startup ownership: no window access or surface reconfiguration.
  if(!open_world_renderer_flush(r) ||
      !r->frame_queue.synchronize(r->queue,frame_fence_timeout_ms()))return false;
  if(!resize_targets(*r,unsigned(r->width),unsigned(r->height))) {
    r->status="temporal_prepare_failed";return false;
  }
  return prepare_rt_shadow_targets(*r) && prepare_map_reflections(*r);
}
bool world_renderer_resize(WorldRenderer& r,int width,int height) {
  if(width<=0 || height<=0) return true;
  if(!open_world_renderer_flush(&r))return false;
  if(!r.frame_queue.synchronize(r.queue,frame_fence_timeout_ms())) {
    r.status="fence_timeout";return false;
  }
  if(!configure_surface(r,static_cast<unsigned>(width),static_cast<unsigned>(height)))return false;
  if((width!=r.width || height!=r.height || !r.targets[0].depth ||
      r.temporal.requested_mode!=r.temporal.mode || r.temporal.reconfigure) &&
      !resize_targets(r,static_cast<unsigned>(width),static_cast<unsigned>(height)))return false;
  r.width=width;r.height=height;
  return prepare_rt_shadow_targets(r);
}
bool world_renderer_create_device(WorldRenderer& r, WorldBootProgressFn progress, void* progress_user, WorldBootMainFn main_thread) {
  const rhi::Feature features[]={rhi::Feature::Surface,rhi::Feature::Rasterization};
  rhi::DeviceDesc desc{};
  desc.requiredFeatures=features;desc.requiredFeatureCount=2;
  const auto* backend=SDL_getenv("OCTARYN_CLIENT_GRAPHICS_API");
  if(!backend || !*backend) {
#if defined(_WIN32)
    backend="dx12";
#elif defined(__APPLE__)
    backend="metal";
#else
    backend="vulkan";
#endif
  }
  if(!std::strcmp(backend,"vulkan")) {
    // SPIR-V 1.5 retains divergent bindless buffer access decorations.
    desc.deviceType=rhi::DeviceType::Vulkan;desc.slang.targetProfile="spirv_1_5";
    desc.slang.targetFlags=SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
  } else if(!std::strcmp(backend,"metal")) {
    desc.deviceType=rhi::DeviceType::Metal;
  } else if(!std::strcmp(backend,"dx12")) {
    desc.deviceType=rhi::DeviceType::D3D12;desc.slang.targetProfile="sm_6_8";
  } else {
    std::fputs("OCTARYN_CLIENT_GRAPHICS_API requires vulkan, dx12 or metal\n",stderr);return false;
  }
  if(!temporal_mode(r.temporal,SDL_getenv("OCTARYN_CLIENT_UPSCALER")))return false;
  desc.enableValidation=SDL_getenv("OCTARYN_CLIENT_RHI_VALIDATION")!=nullptr;
  desc.debugCallback=&r.device_attempt;
  if(desc.enableValidation) {
    rhi::DebugLayerOptions validation{};validation.coreValidation=true;validation.required=true;
    if(!world_rhi_ok(rhi::getRHI()->setDebugLayerOptions(validation))) {r.status="validation_setup_failed";return false;}
    std::puts("world_validation core=required");
  }
  const auto shader_caches=configure_shader_caches(desc);
  const auto original_bindless=desc.bindless;
  desc.bindless.bufferCount=WorldDescriptorCapacity;
  bool batch_capacity=true;
  if(!world_rhi_ok(rhi::getRHI()->createDevice(desc,r.device.writeRef()))) {
    std::puts("world_batch_capacity expanded_device_failed retry=default_descriptors");
    r.device.setNull();desc.bindless=original_bindless;batch_capacity=false;
    desc.debugCallback=&r.debug;
    if(!world_rhi_ok(rhi::getRHI()->createDevice(desc,r.device.writeRef()))) {r.status="rhi_device_failed";return false;}
  }
  if(batch_capacity)r.device_attempt.accept();
  r.capabilities=renderer_capabilities(r.device,desc.bindless);
  print_renderer_capabilities(r.capabilities);
  if(r.debug.errors.load()!=0) {r.status="rhi_device_validation_failed";return false;}
  const auto& device_info=r.device->getInfo();
  std::printf("world_device backend=slang_rhi api=%s adapter=%s\n",
      device_info.apiName?device_info.apiName:"unknown",device_info.adapterName?device_info.adapterName:"unknown");
  if(progress)progress("graphics device",progress_user);
  if(!world_rhi_ok(r.device->getQueue(rhi::QueueType::Graphics,r.queue.writeRef()))) return false;
  const auto* frame_mode=SDL_getenv("OCTARYN_CLIENT_FRAMES_IN_FLIGHT");
  if(frame_mode && std::strcmp(frame_mode,"1") && std::strcmp(frame_mode,"2")) {
    std::fputs("OCTARYN_CLIENT_FRAMES_IN_FLIGHT requires 1 or 2\n",stderr);return false;
  }
  const unsigned frame_count=frame_mode && !std::strcmp(frame_mode,"1")?1u:2u;
  if(!r.frame_queue.initialize(r.device,frame_count))return false;
  std::printf("world_frames count=%u mutable_targets=per_slot\n",frame_count);
  r.status="window_surface";
  const auto surface_operation=[](void* argument) {
    auto& renderer=*static_cast<WorldRenderer*>(argument);
    rhi::WindowHandle handle{};
    if(window_handle(renderer.window,handle))
      renderer.device->createSurface(handle,renderer.surface.writeRef());
  };
  if(main_thread)main_thread(surface_operation,&r,progress_user);
  else surface_operation(&r);
  if(!r.surface) {
    r.status="rhi_surface_create_failed";return false;
  }
  const auto& info=r.surface->getInfo();
  // Presentation is written by compute before the UI pass; sRGB formats cannot
  // be storage images. X11 surfaces commonly expose BGRA rather than RGBA.
  r.color_format=rhi::Format::Undefined;
  for(auto preferred:{rhi::Format::BGRA8Unorm,rhi::Format::RGBA8Unorm})
    for(std::uint32_t i=0;i<info.formatCount;++i)
      if(info.formats[i]==preferred)r.color_format=preferred;
  if(r.color_format==rhi::Format::Undefined) {
    r.status="surface_missing_linear_rgba_bgra_format";return false;
  }

  r.status="ui_renderer";
  if(progress)progress("interface",progress_user);
  r.ui_renderer=create_rml_renderer(r.device,r.color_format);
  if(!r.ui_renderer)return false;
  r.status="atlas";
  if(progress)progress("material atlas",progress_user);
  r.atlas=create_world_atlas(r.device);
  if(!r.atlas) {r.status="atlas_create_failed";return false;}
  r.status="sky_world_hdr_pipelines";
  if(progress)progress("sky and HDR",progress_user);
  const auto sky_path=resolve_slang_shader_path("octaryn-client/Shaders/Sky/Sky.slang");
  if(sky_path.empty() ||
     !create_sky_pipeline(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,sky_path.c_str(),r.sky_pipeline) ||
     !create_world_hdr(r.device,r.targets[0].hdr)) return false;
  for(unsigned slot=1;slot<frame_count;++slot) {
    r.targets[slot].hdr.composite=r.targets[0].hdr.composite;
    r.targets[slot].hdr.composite_rt=r.targets[0].hdr.composite_rt;
    r.targets[slot].hdr.present=r.targets[0].hdr.present;
  }
  r.status="cloud_pipeline";
  if(progress)progress("clouds",progress_user);
  const auto cloud_path=resolve_slang_shader_path("octaryn-client/Shaders/Sky/Clouds.slang");
  if(!create_cloud_pipeline(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,cloud_path.c_str(),r.cloud_pipeline)) return false;
  if(progress)progress("ray tracing resources",progress_user);
  if(!world_ray_initialize(r))return false;
  if(progress)progress("ray lighting pipelines",progress_user);
  if(!world_ray_lighting_initialize(r))return false;
  if(progress)progress("lighting resources",progress_user);
  if(!initialize_lighting(r))return false;
  if(progress)progress("surface targets",progress_user);
  if(!open_world_renderer_flush(&r) || !r.frame_queue.synchronize(r.queue,frame_fence_timeout_ms()))return false;
  struct Resize {WorldRenderer* renderer;int width{},height{};bool ready{};Uint64 elapsed_ns{};} resize{&r};
  const auto resize_operation=[](void* argument) {
    auto& request=*static_cast<Resize*>(argument);
    const auto started=SDL_GetTicksNS();
    SDL_GetWindowSizeInPixels(request.renderer->window,&request.width,&request.height);
    request.ready=request.width<=0 || request.height<=0 || configure_surface(*request.renderer,
        static_cast<unsigned>(request.width),static_cast<unsigned>(request.height));
    request.elapsed_ns=SDL_GetTicksNS()-started;
  };
  if(main_thread)main_thread(resize_operation,&resize,progress_user);
  else resize_operation(&resize);
  if(!resize.ready || resize.width<=0 || resize.height<=0)return resize.ready;
  // Surface/window work has returned to its owner. FSR, HDR and GI allocations
  // remain on the exclusive startup worker while the main thread pumps events.
  const auto allocation_started=SDL_GetTicksNS();
  if(!resize_targets(r,static_cast<unsigned>(resize.width),static_cast<unsigned>(resize.height)))return false;
  r.width=resize.width;r.height=resize.height;
  std::printf("client_boot_targets surface_ms=%.3f allocation_ms=%.3f\n",double(resize.elapsed_ns)/1e6,
      double(SDL_GetTicksNS()-allocation_started)/1e6);
  return true;
}
}
