#include "app/runtime/renderdoc_capture.h"

#include <cstdint>
#include <cstdlib>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <SDL3/SDL.h>

#include "core/log.h"
#include "core/profile.h"

namespace {

using renderdoc_trigger_capture_fn = void (*)();

struct renderdoc_api_v100 {
    void (*GetAPIVersion)(int*, int*, int*);
    void (*SetCaptureOptionU32)(int, uint32_t);
    void (*SetCaptureOptionF32)(int, float);
    uint32_t (*GetCaptureOptionU32)(int);
    float (*GetCaptureOptionF32)(int);
    void (*SetFocusToggleKeys)(void*, int);
    void (*SetCaptureKeys)(void*, int);
    uint32_t (*GetOverlayBits)();
    void (*MaskOverlayBits)(uint32_t, uint32_t);
    void (*Shutdown)();
    void (*UnloadCrashHandler)();
    void (*SetLogFilePathTemplate)(const char*);
    const char* (*GetLogFilePathTemplate)();
    uint32_t (*GetNumCaptures)();
    uint32_t (*GetCapture)(uint32_t, char*, uint32_t*, uint64_t*);
    renderdoc_trigger_capture_fn TriggerCapture;
};

using renderdoc_get_api_fn = int (*)(uint32_t, void**);

constexpr uint32_t RENDERDOC_API_VERSION_1_0_0 = 10000u;

bool g_enabled = false;
bool g_configured = false;
bool g_unavailable_logged = false;
bool g_triggered = false;
uint32_t g_capture_frame = 500u;
uint32_t g_render_frame = 0u;
renderdoc_api_v100* g_api = nullptr;

bool env_flag_enabled(const char* name)
{
    const char* value = SDL_getenv(name);
    if (!value || value[0] == '\0')
    {
        return false;
    }
    return SDL_strcasecmp(value, "0") != 0 &&
           SDL_strcasecmp(value, "false") != 0 &&
           SDL_strcasecmp(value, "off") != 0 &&
           SDL_strcasecmp(value, "no") != 0;
}

uint32_t env_uint_value(const char* name, uint32_t fallback, uint32_t minimum, uint32_t maximum)
{
    const char* value = SDL_getenv(name);
    if (!value || value[0] == '\0')
    {
        return fallback;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value)
    {
        return fallback;
    }
    if (parsed < minimum)
    {
        return minimum;
    }
    if (parsed > maximum)
    {
        return maximum;
    }
    return static_cast<uint32_t>(parsed);
}

renderdoc_api_v100* find_renderdoc_api()
{
#if defined(_WIN32)
    HMODULE renderdoc_module = GetModuleHandleA("renderdoc.dll");
    if (!renderdoc_module)
    {
        return nullptr;
    }
    FARPROC symbol = GetProcAddress(renderdoc_module, "RENDERDOC_GetAPI");
#else
    void* symbol = dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
#endif
    if (!symbol)
    {
        return nullptr;
    }

#if defined(_WIN32) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
    auto get_api = reinterpret_cast<renderdoc_get_api_fn>(symbol);
#if defined(_WIN32) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    void* api = nullptr;
    if (get_api(RENDERDOC_API_VERSION_1_0_0, &api) != 1 || !api)
    {
        return nullptr;
    }
    return static_cast<renderdoc_api_v100*>(api);
}

} // namespace

void app_renderdoc_capture_init(void)
{
    if (g_configured)
    {
        return;
    }
    g_configured = true;
    g_enabled = env_flag_enabled("OCTARYN_RENDERDOC_CAPTURE");
    if (!g_enabled)
    {
        return;
    }

    g_capture_frame = env_uint_value("OCTARYN_RENDERDOC_CAPTURE_FRAME", 500u, 1u, 1000000u);
    g_api = find_renderdoc_api();
    if (!g_api || !g_api->TriggerCapture)
    {
        oct_log_warnf("RenderDoc auto capture requested but RENDERDOC_GetAPI is unavailable");
        g_unavailable_logged = true;
        return;
    }

    oct_log_infof("RenderDoc auto capture enabled: frame=%u", g_capture_frame);
}

void app_renderdoc_capture_before_render(void)
{
    if (!g_enabled || g_triggered)
    {
        return;
    }
    OCT_PROFILE_ZONE("renderdoc_capture_before_render");

    ++g_render_frame;
    if (!g_api)
    {
        g_api = find_renderdoc_api();
        if (!g_api || !g_api->TriggerCapture)
        {
            if (!g_unavailable_logged)
            {
                oct_log_warnf("RenderDoc auto capture waiting for RENDERDOC_GetAPI");
                g_unavailable_logged = true;
            }
            return;
        }
        oct_log_infof("RenderDoc auto capture connected: frame=%u", g_capture_frame);
    }

    if (g_render_frame < g_capture_frame)
    {
        return;
    }

    g_api->TriggerCapture();
    g_triggered = true;
    oct_log_infof("RenderDoc auto capture triggered: frame=%u", g_render_frame);
}
