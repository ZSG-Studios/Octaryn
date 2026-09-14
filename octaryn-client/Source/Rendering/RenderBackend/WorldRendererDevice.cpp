#include "WorldRendererInternal.h"
#include "WorldRasterPipeline.h"
#include "SlangShaderPath.h"
#include "AssetPath.h"
#include <cstdio>
#include <cstring>
namespace octaryn::client::rendering {
namespace {
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
bool world_renderer_resize(WorldRenderer& r,int width,int height) {
  if(width<=0 || height<=0) return true;
  if(!open_world_renderer_flush(&r))return false;
  if(!world_rhi_ok(r.queue->waitOnHost())) return false;
  rhi::SurfaceConfig config{};
  config.format=r.color_format;config.usage=rhi::TextureUsage::CopyDestination;
  config.width=static_cast<std::uint32_t>(width);config.height=static_cast<std::uint32_t>(height);
  config.desiredImageCount=2;config.vsync=false;
  if(!world_rhi_ok(r.surface->configure(config))) {r.status="surface_configure_failed";return false;}
  if(!resize_temporal(r.temporal,r.device,config.width,config.height,r.frame_queue.count()))return false;
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
  desc.size={config.width,config.height,1};
  desc.usage=rhi::TextureUsage::RenderTarget|rhi::TextureUsage::UnorderedAccess|
      rhi::TextureUsage::CopySource|rhi::TextureUsage::CopyDestination;
  desc.defaultState=rhi::ResourceState::RenderTarget;desc.label="world_color";
  if(!world_rhi_ok(r.device->createTexture(desc,nullptr,target.color.writeRef())) ||
     !world_rhi_ok(target.color->getDefaultView(target.color_view.writeRef()))) return false;
  if(!resize_world_hdr(r.device,target.hdr,r.temporal.allocation_width,r.temporal.allocation_height)) return false;
  }
  r.width=width;r.height=height;
  return true;
}
bool world_renderer_create_device(WorldRenderer& r) {
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
    desc.deviceType=rhi::DeviceType::Vulkan;desc.slang.targetProfile="spirv_1_3";
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
  const auto original_bindless=desc.bindless;
  desc.bindless.bufferCount=WorldBatchDescriptorCapacity;
  bool batch_capacity=true;
  if(!world_rhi_ok(rhi::getRHI()->createDevice(desc,r.device.writeRef()))) {
    std::puts("world_batch_capacity expanded_device_failed retry=default_descriptors");
    r.device.setNull();desc.bindless=original_bindless;batch_capacity=false;
    desc.debugCallback=&r.debug;
    if(!world_rhi_ok(rhi::getRHI()->createDevice(desc,r.device.writeRef()))) {r.status="rhi_device_failed";return false;}
  }
  if(batch_capacity)r.device_attempt.accept();
  if(r.debug.errors.load()!=0) {r.status="rhi_device_validation_failed";return false;}
  const auto& device_info=r.device->getInfo();
  std::printf("world_device backend=slang_rhi api=%s adapter=%s\n",
      device_info.apiName?device_info.apiName:"unknown",device_info.adapterName?device_info.adapterName:"unknown");
  if(!world_rhi_ok(r.device->getQueue(rhi::QueueType::Graphics,r.queue.writeRef()))) return false;
  const auto* frame_mode=SDL_getenv("OCTARYN_CLIENT_FRAMES_IN_FLIGHT");
  if(frame_mode && std::strcmp(frame_mode,"1") && std::strcmp(frame_mode,"2")) {
    std::fputs("OCTARYN_CLIENT_FRAMES_IN_FLIGHT requires 1 or 2\n",stderr);return false;
  }
  const unsigned frame_count=frame_mode && !std::strcmp(frame_mode,"1")?1u:2u;
  if(!r.frame_queue.initialize(r.device,frame_count))return false;
  std::printf("world_frames count=%u mutable_targets=per_slot\n",frame_count);
  r.status="window_surface";
  rhi::WindowHandle handle{};
  if(!window_handle(r.window,handle) || !world_rhi_ok(r.device->createSurface(handle,r.surface.writeRef()))) {
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

  r.status="mesh_pipeline";
  if(!create_rhi_compute_pipeline(r.device,"octaryn-client/Shaders/Voxel/WorldFaces.slang","main",r.mesh_pipeline)) return false;
  r.status="atlas";
  r.atlas=create_world_atlas(r.device);
  if(!r.atlas) {r.status="atlas_create_failed";return false;}
  r.status="sky_world_hdr_pipelines";
  const auto sky_path=resolve_slang_shader_path("octaryn-client/Shaders/Sky/Sky.slang");
  if(sky_path.empty() ||
     !create_sky_pipeline(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,sky_path.c_str(),r.sky_pipeline) ||
     !create_world_raster_pipelines(r.device,r.raster_pipeline,r.sprite_pipeline,r.lava_pipeline,r.transparent_pipeline) ||
     !create_world_hdr(r.device,r.targets[0].hdr)) return false;
  for(unsigned slot=1;slot<frame_count;++slot) {
    r.targets[slot].hdr.composite=r.targets[0].hdr.composite;
    r.targets[slot].hdr.present=r.targets[0].hdr.present;
  }
  r.status="cloud_pipeline";
  const auto cloud_path=resolve_slang_shader_path("octaryn-client/Shaders/Sky/Clouds.slang");
  if(!create_cloud_pipeline(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,cloud_path.c_str(),r.cloud_pipeline)) return false;
  r.status="player_asset_path";
  char player_path[4096]{};
  if(!bundle_path_build(player_path,sizeof(player_path),"Client/Assets/Player/octaryn_player_v1.gltf")) return false;
  const auto player_shader=resolve_slang_shader_path("octaryn-client/Shaders/Player/Player.slang");
  r.status="player_renderer";
  r.player=create_player_renderer(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,player_path,player_shader.c_str());
  r.status="ui_renderer";
  r.ui_renderer=create_rml_renderer(r.device,r.color_format);
  const auto item_shader=resolve_slang_shader_path("octaryn-client/Shaders/WorldItems/WorldItems.slang");
  r.status="world_items_renderer";
  r.items=create_world_items_renderer(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,item_shader.c_str());
  r.status="player_ui_items_selection";
  if(!r.player || !r.ui_renderer || !r.items || !create_selection_pipeline(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,r.selection_pipeline)) return false;
  r.status="batch_and_surface_resize";
  int width{},height{};SDL_GetWindowSizeInPixels(r.window,&width,&height);
  return world_batch_initialize(r,batch_capacity) && world_renderer_resize(r,width,height);
}
}
