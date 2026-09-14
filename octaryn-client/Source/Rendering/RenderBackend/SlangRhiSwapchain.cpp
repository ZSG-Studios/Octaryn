#include "SlangRhiSwapchain.h"

#include <SDL3/SDL.h>

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <slang-com-ptr.h>
#include <slang-gfx.h>
#endif

#include <cstdint>

namespace octaryn::client::rendering {

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
namespace {

struct SwapchainResources {
  Slang::ComPtr<gfx::IDevice> device;
  Slang::ComPtr<gfx::ICommandQueue> queue;
  Slang::ComPtr<gfx::ISwapchain> swapchain;
  Slang::ComPtr<gfx::ITransientResourceHeap> heap;
  Slang::ComPtr<gfx::ICommandBuffer> commands;
  Slang::ComPtr<gfx::ITextureResource> image;
  Slang::ComPtr<gfx::IResourceView> image_view;

  ~SwapchainResources() {
    if (queue != nullptr) { queue->waitOnHost(); }
  }
};

bool native_window_handle(SDL_Window *window, gfx::WindowHandle &handle) {
  const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
#if defined(_WIN32)
  void *hwnd = SDL_GetPointerProperty(
      properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
  if (hwnd == nullptr) { return false; }
  handle = gfx::WindowHandle::FromHwnd(hwnd);
#elif defined(__APPLE__)
  void *nswindow = SDL_GetPointerProperty(
      properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
  if (nswindow == nullptr) { return false; }
  handle = gfx::WindowHandle::FromNSWindow(nswindow);
#else
  void *xdisplay = SDL_GetPointerProperty(
      properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
  const std::int64_t xwindow = SDL_GetNumberProperty(
      properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
  if (xdisplay == nullptr || xwindow == 0) { return false; }
  handle = gfx::WindowHandle::FromXWindow(
      xdisplay, static_cast<std::uint32_t>(xwindow));
#endif
  return true;
}

bool encode_present_image(SwapchainResources &gpu,
                          SlangRhiSwapchainProbeResult &probe) {
  gfx::ITransientResourceHeap::Desc heap_desc{};
  heap_desc.constantBufferSize = 4096;
  if (SLANG_FAILED(gpu.device->createTransientResourceHeap(
          heap_desc, gpu.heap.writeRef())) || gpu.heap == nullptr ||
      SLANG_FAILED(gpu.heap->synchronizeAndReset())) {
    probe.status = "transient_heap_create_failed";
    return false;
  }
  if (SLANG_FAILED(gpu.heap->createCommandBuffer(gpu.commands.writeRef())) ||
      gpu.commands == nullptr) {
    probe.status = "command_buffer_create_failed";
    return false;
  }

  gfx::IResourceView::Desc view_desc{};
  view_desc.type = gfx::IResourceView::Type::RenderTarget;
  view_desc.format = gpu.swapchain->getDesc().format;
  view_desc.renderTarget.shape = gfx::IResource::Type::Texture2D;
  view_desc.subresourceRange.aspectMask = gfx::TextureAspect::Color;
  view_desc.subresourceRange.mipLevelCount = 1;
  view_desc.subresourceRange.layerCount = 1;
  if (SLANG_FAILED(gpu.device->createTextureView(
          gpu.image, view_desc, gpu.image_view.writeRef())) ||
      gpu.image_view == nullptr) {
    probe.status = "swapchain_image_view_failed";
    return false;
  }

  gfx::IResourceCommandEncoder *encoder = nullptr;
  gpu.commands->encodeResourceCommands(&encoder);
  if (encoder == nullptr) {
    probe.status = "resource_encoder_create_failed";
    return false;
  }
  // Include GFX's BOTTOM_OF_PIPE acquire wait in the execution dependency
  // before the initial layout transition.
  encoder->bufferBarrier(0, nullptr, gfx::ResourceState::Present,
                           gfx::ResourceState::General);
  // This probe acquires a newly created swapchain image exactly once.
  encoder->textureBarrier(gpu.image, gfx::ResourceState::Undefined,
                           gfx::ResourceState::RenderTarget);
  gfx::ClearValue clear{};
  clear.color.floatValues[0] = 0.05f;
  clear.color.floatValues[1] = 0.10f;
  clear.color.floatValues[2] = 0.15f;
  clear.color.floatValues[3] = 1.0f;
  encoder->clearResourceView(gpu.image_view, &clear,
                             gfx::ClearResourceViewFlags::None);
  encoder->textureBarrier(gpu.image, gfx::ResourceState::RenderTarget,
                           gfx::ResourceState::Present);
  encoder->endEncoding();
  encoder->release();
  gpu.commands->close();
  probe.image_transitioned = true;
  return true;
}

void probe_window_swapchain(SDL_Window *window,
                            SlangRhiSwapchainProbeResult &probe) {
  gfx::WindowHandle handle{};
  if (!native_window_handle(window, handle)) {
    probe.status = "native_window_handle_unavailable";
    return;
  }
  probe.native_handle_available = true;

  SwapchainResources gpu;
  gfx::IDevice::Desc device_desc{};
  device_desc.deviceType = gfx::DeviceType::Vulkan;
  device_desc.slang.targetFlags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
  if (SLANG_FAILED(gfx::gfxCreateDevice(&device_desc, gpu.device.writeRef())) ||
      gpu.device == nullptr) {
    probe.status = "device_create_failed";
    return;
  }
  probe.device_created = true;
  gfx::ICommandQueue::Desc queue_desc{};
  queue_desc.type = gfx::ICommandQueue::QueueType::Graphics;
  if (SLANG_FAILED(gpu.device->createCommandQueue(
          queue_desc, gpu.queue.writeRef())) || gpu.queue == nullptr) {
    probe.status = "queue_create_failed";
    return;
  }
  probe.queue_created = true;

  gfx::ISwapchain::Desc swapchain_desc{};
  swapchain_desc.format = gfx::Format::R8G8B8A8_UNORM;
  swapchain_desc.width = 64;
  swapchain_desc.height = 64;
  swapchain_desc.imageCount = 2;
  swapchain_desc.queue = gpu.queue;
  swapchain_desc.enableVSync = false;
  if (SLANG_FAILED(gpu.device->createSwapchain(
          swapchain_desc, handle, gpu.swapchain.writeRef())) ||
      gpu.swapchain == nullptr) {
    probe.status = "swapchain_create_failed";
    return;
  }
  probe.swapchain_created = true;

  const int image_index = gpu.swapchain->acquireNextImage();
  if (image_index < 0 || SLANG_FAILED(gpu.swapchain->getImage(
          image_index, gpu.image.writeRef())) || gpu.image == nullptr) {
    probe.status = "swapchain_image_acquire_failed";
    return;
  }
  probe.image_acquired = true;
  if (!encode_present_image(gpu, probe)) { return; }
  gpu.queue->executeCommandBuffer(gpu.commands);
  probe.command_submitted = true;
  if (SLANG_FAILED(gpu.swapchain->present())) {
    probe.status = "swapchain_present_failed";
    return;
  }
  probe.presented = true;
  gpu.queue->waitOnHost();
  probe.queue_idle = true;
  if (SLANG_FAILED(gpu.heap->finish())) {
    probe.status = "transient_heap_finish_failed";
    return;
  }
  probe.status = "swapchain_present_validated";
}

} // namespace
#endif

SlangRhiSwapchainProbeResult probe_slang_rhi_swapchain() {
  SlangRhiSwapchainProbeResult probe{};
  probe.video_driver = "uninitialized";
  probe.status = "not_started";
#if !defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
  probe.status = "runtime_unavailable";
#else
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
    probe.status = "sdl_video_init_failed";
    return probe;
  }
  probe.sdl_video_initialized = true;
  probe.video_driver = SDL_GetCurrentVideoDriver();
  if (probe.video_driver == nullptr) { probe.video_driver = "unknown"; }
  SDL_Window *window = SDL_CreateWindow(
      "Octaryn Slang Vulkan swapchain probe", 64, 64, SDL_WINDOW_HIDDEN);
  if (window == nullptr) {
    probe.status = "window_create_failed";
  } else {
    probe.window_created = true;
    probe_window_swapchain(window, probe);
    // All GPU resources and the Vulkan surface have already been released.
    SDL_DestroyWindow(window);
  }
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif
  return probe;
}

} // namespace octaryn::client::rendering
