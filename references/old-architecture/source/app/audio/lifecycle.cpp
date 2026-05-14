#include "internal.h"

#include "core/log.h"
#include "core/profile.h"

using namespace app_audio_internal;

void oct_audio_init(void)
{
    OCT_PROFILE_ZONE("oct_audio_init");
    const Uint64 start_ticks = oct_profile_now_ticks();
    std::scoped_lock lock(g_audio.mutex);
    if (g_audio.initialized)
    {
        return;
    }
    g_audio.device = alcOpenDevice(nullptr);
    if (!g_audio.device)
    {
        oct_log_warnf("OpenAL device unavailable; audio disabled");
        return;
    }
    g_audio.context = alcCreateContext(g_audio.device, nullptr);
    if (!g_audio.context || !alcMakeContextCurrent(g_audio.context))
    {
        oct_log_warnf("OpenAL context unavailable; audio disabled");
        if (g_audio.context)
        {
            alcDestroyContext(g_audio.context);
            g_audio.context = nullptr;
        }
        alcCloseDevice(g_audio.device);
        g_audio.device = nullptr;
        return;
    }
    alGenBuffers(static_cast<ALsizei>(g_audio.buffers.size()), g_audio.buffers.data());
    alGenSources(static_cast<ALsizei>(g_audio.sources.size()), g_audio.sources.data());
    if (alGetError() != AL_NO_ERROR)
    {
        oct_log_warnf("Failed to initialize OpenAL sources/buffers; audio disabled");
        oct_audio_shutdown();
        return;
    }
    for (std::size_t i = 0; i < g_audio.buffers.size(); i++)
    {
        if (!create_buffer_for_event(static_cast<oct_audio_event_t>(i), g_audio.buffers[i]))
        {
            oct_log_warnf("Failed to build audio buffer %d; audio disabled", static_cast<int>(i));
            oct_audio_shutdown();
            return;
        }
    }
    g_audio.initialized = true;
    oct_log_infof("Audio initialized with OpenAL + miniaudio synthesis");
    oct_profile_log_duration("Startup timing", "oct_audio_init internals", start_ticks);
}

void oct_audio_shutdown(void)
{
    std::scoped_lock lock(g_audio.mutex);
    if (!g_audio.device && !g_audio.context)
    {
        return;
    }
    if (g_audio.context)
    {
        alDeleteSources(static_cast<ALsizei>(g_audio.sources.size()), g_audio.sources.data());
        alDeleteBuffers(static_cast<ALsizei>(g_audio.buffers.size()), g_audio.buffers.data());
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(g_audio.context);
    }
    if (g_audio.device)
    {
        alcCloseDevice(g_audio.device);
    }
    g_audio.device = nullptr;
    g_audio.context = nullptr;
    g_audio.buffers.fill(0);
    g_audio.sources.fill(0);
    g_audio.initialized = false;
}
