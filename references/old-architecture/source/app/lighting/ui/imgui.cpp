#include "imgui.h"

#include "internal.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

namespace app_lighting_ui_internal {

bool g_initialized = false;
} // namespace app_lighting_ui_internal

bool main_imgui_lighting_init(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat swapchain_format)
{
    if (app_lighting_ui_internal::g_initialized)
    {
        return true;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = device;
    init_info.ColorTargetFormat = swapchain_format;
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    init_info.PresentMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    ImGui_ImplSDLGPU3_Init(&init_info);
    app_lighting_ui_internal::g_initialized = true;
    return true;
}

void main_imgui_lighting_shutdown(void)
{
    if (!app_lighting_ui_internal::g_initialized)
    {
        return;
    }
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    app_lighting_ui_internal::g_initialized = false;
}

void main_imgui_lighting_process_event(const SDL_Event* event)
{
    if (app_lighting_ui_internal::g_initialized)
    {
        ImGui_ImplSDL3_ProcessEvent(event);
    }
}

bool main_imgui_lighting_wants_capture(void)
{
    if (!app_lighting_ui_internal::g_initialized)
    {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse || io.WantCaptureKeyboard;
}

void main_imgui_lighting_render(SDL_GPUCommandBuffer* command_buffer, SDL_GPUTexture* target_texture)
{
    if (!target_texture)
    {
        return;
    }

    ImDrawData* draw_data = app_lighting_ui_internal::g_initialized ? ImGui::GetDrawData() : nullptr;
    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture = target_texture;
    target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op = SDL_GPU_STOREOP_STORE;
    target_info.clear_color.r = 0.0f;
    target_info.clear_color.g = 0.0f;
    target_info.clear_color.b = 0.0f;
    target_info.clear_color.a = 0.0f;
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
    if (render_pass)
    {
        if (draw_data && draw_data->DisplaySize.x > 0.0f && draw_data->DisplaySize.y > 0.0f)
        {
            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);
            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
        }
        SDL_EndGPURenderPass(render_pass);
    }
}
