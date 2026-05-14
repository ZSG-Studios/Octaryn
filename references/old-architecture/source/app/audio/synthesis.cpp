#include "internal.h"

#include <algorithm>
#include <array>

#include "core/log.h"

namespace app_audio_internal {

auto event_frequency(oct_audio_event_t event) -> double
{
    switch (event)
    {
    case OCT_AUDIO_EVENT_PLACE: return 540.0;
    case OCT_AUDIO_EVENT_BREAK: return 180.0;
    case OCT_AUDIO_EVENT_SELECT: return 760.0;
    case OCT_AUDIO_EVENT_CHANGE: return 620.0;
    }
    return 440.0;
}

auto event_gain(oct_audio_event_t event) -> double
{
    switch (event)
    {
    case OCT_AUDIO_EVENT_BREAK: return 0.35;
    case OCT_AUDIO_EVENT_SELECT: return 0.12;
    case OCT_AUDIO_EVENT_CHANGE: return 0.10;
    case OCT_AUDIO_EVENT_PLACE:
    default: return 0.18;
    }
}

bool create_buffer_for_event(oct_audio_event_t event, ALuint buffer)
{
    std::array<float, k_duration_frames> samples{};
    std::array<short, k_duration_frames> pcm16{};
    ma_waveform_config config = ma_waveform_config_init(
        ma_format_f32, k_channels, k_sample_rate, ma_waveform_type_sine, event_gain(event), event_frequency(event));
    ma_waveform waveform{};
    if (ma_waveform_init(&config, &waveform) != MA_SUCCESS)
    {
        oct_log_errorf("Failed to initialize miniaudio waveform for event %d", static_cast<int>(event));
        return false;
    }
    ma_uint64 frames_read = 0;
    if (ma_waveform_read_pcm_frames(&waveform, samples.data(), k_duration_frames, &frames_read) != MA_SUCCESS ||
        frames_read != k_duration_frames)
    {
        oct_log_errorf("Failed to generate waveform PCM for event %d", static_cast<int>(event));
        return false;
    }
    for (ma_uint64 i = 0; i < frames_read; i++)
    {
        const float fade = 1.0f - static_cast<float>(i) / static_cast<float>(frames_read);
        samples[static_cast<std::size_t>(i)] *= fade * fade;
        const float clamped = std::clamp(samples[static_cast<std::size_t>(i)], -1.0f, 1.0f);
        pcm16[static_cast<std::size_t>(i)] = static_cast<short>(clamped * 32767.0f);
    }
    alBufferData(
        buffer, AL_FORMAT_MONO16, pcm16.data(), static_cast<ALsizei>(frames_read * sizeof(short)),
        static_cast<ALsizei>(k_sample_rate));
    return alGetError() == AL_NO_ERROR;
}

} // namespace app_audio_internal
