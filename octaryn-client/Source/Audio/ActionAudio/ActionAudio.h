#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace octaryn::client::audio {
inline constexpr std::uint32_t ActionSampleRate=48000;
inline constexpr std::size_t ActionSampleCount=4000;
inline constexpr std::size_t ActionVoiceCount=8;
enum class ActionSound { Place, Break, Select, Change, UiHover, UiClick, UiChange, Count };
inline constexpr std::size_t ActionSoundCount=static_cast<std::size_t>(ActionSound::Count);
struct SoundDefinition {
  double frequency{},gain{};
  double duration_ms{1000.0*ActionSampleCount/ActionSampleRate};
  double attack_ms{},fade_power{2};
};
using SoundDefinitions=std::array<SoundDefinition,ActionSoundCount>;
using ActionSamples=std::array<std::int16_t,ActionSampleCount>;
enum class OutputMode { DefaultDevice, Loopback };
enum class PlayResult { Played, Busy, Unavailable, Invalid };
struct ActionAudioStatus {
  bool available{};
  std::uint32_t active_voices{};
  std::uint64_t played{},dropped{};
  const char* message{};
};
struct ActionAudio;
// All device operations stay on the creating application thread.
// Unavailable devices return an owner with a diagnostic status, never a fallback.
ActionAudio* create_action_audio(const SoundDefinitions&,OutputMode=OutputMode::DefaultDevice);
void destroy_action_audio(ActionAudio*);
PlayResult play_action_audio(ActionAudio*,ActionSound,bool loop=false);
bool stop_action_audio(ActionAudio*);
ActionAudioStatus action_audio_status(ActionAudio*);
bool valid_action_sound(const SoundDefinition&);
bool synthesize_action_sound(const SoundDefinition&,ActionSamples&);
// Explicit loopback-only output: does not create or route to a speaker device.
bool render_action_audio_loopback(ActionAudio*,std::span<std::int16_t>);
struct ActionAudioDeleter { void operator()(ActionAudio* value) const {destroy_action_audio(value);} };
using ActionAudioOwner=std::unique_ptr<ActionAudio,ActionAudioDeleter>;
}
