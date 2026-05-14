#pragma once

#include "audio.h"

#include <array>
#include <mutex>

#include <AL/al.h>
#include <AL/alc.h>
#include <miniaudio.h>

namespace app_audio_internal {

inline constexpr ma_uint32 k_sample_rate = 48000;
inline constexpr ma_uint32 k_channels = 1;
inline constexpr ma_uint64 k_duration_frames = k_sample_rate / 12;
inline constexpr int k_source_count = 8;

struct audio_state
{
    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;
    std::array<ALuint, 4> buffers{};
    std::array<ALuint, k_source_count> sources{};
    bool initialized = false;
    std::mutex mutex{};
};

extern audio_state g_audio;

auto event_frequency(oct_audio_event_t event) -> double;
auto event_gain(oct_audio_event_t event) -> double;
bool create_buffer_for_event(oct_audio_event_t event, ALuint buffer);

} // namespace app_audio_internal
