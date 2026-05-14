#include "internal.h"

using namespace app_audio_internal;

void oct_audio_play_event(oct_audio_event_t event)
{
    std::scoped_lock lock(g_audio.mutex);
    if (!g_audio.initialized)
    {
        return;
    }
    for (ALuint source : g_audio.sources)
    {
        ALint state = 0;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING)
        {
            continue;
        }
        alSourceStop(source);
        alSourcei(source, AL_BUFFER, static_cast<ALint>(g_audio.buffers[static_cast<std::size_t>(event)]));
        alSourcef(source, AL_GAIN, 1.0f);
        alSourcePlay(source);
        return;
    }
}
