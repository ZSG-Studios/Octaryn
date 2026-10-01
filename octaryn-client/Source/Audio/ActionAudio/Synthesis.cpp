#include "ActionAudio.h"
#include <miniaudio.h>
#include <algorithm>
#include <cmath>

namespace octaryn::client::audio {
bool valid_action_sound(const SoundDefinition& definition) {
  return std::isfinite(definition.frequency) && definition.frequency>0 &&
      definition.frequency<ActionSampleRate/2 && std::isfinite(definition.gain) &&
      definition.gain>=0 && definition.gain<=4 &&
      std::isfinite(definition.duration_ms) && definition.duration_ms>=1 &&
      definition.duration_ms<=1000.0*ActionSampleCount/ActionSampleRate &&
      std::isfinite(definition.attack_ms) && definition.attack_ms>=0 &&
      definition.attack_ms<definition.duration_ms &&
      std::isfinite(definition.fade_power) && definition.fade_power>=1 && definition.fade_power<=8;
}
bool synthesize_action_sound(const SoundDefinition& definition,ActionSamples& output) {
  output.fill(0);
  if(!valid_action_sound(definition)) return false;
  const auto frames=static_cast<std::size_t>(std::lround(definition.duration_ms*ActionSampleRate/1000));
  const auto attack=definition.attack_ms*ActionSampleRate/1000;
  std::array<float,ActionSampleCount> samples{};
  const auto config=ma_waveform_config_init(ma_format_f32,1,ActionSampleRate,
      ma_waveform_type_sine,definition.gain,definition.frequency);
  ma_waveform waveform{};
  if(ma_waveform_init(&config,&waveform)!=MA_SUCCESS) return false;
  ma_uint64 count{};
  const auto result=ma_waveform_read_pcm_frames(&waveform,samples.data(),frames,&count);
  ma_waveform_uninit(&waveform);
  if(result!=MA_SUCCESS || count!=frames) return false;
  for(std::size_t i=0;i<frames;++i) {
    const float fade=1-static_cast<float>(i)/static_cast<float>(frames);
    const auto release=definition.fade_power==2?fade*fade:std::pow(fade,static_cast<float>(definition.fade_power));
    const auto onset=attack>0?static_cast<float>(std::min(1.0,static_cast<double>(i)/attack)):1.f;
    output[i]=static_cast<std::int16_t>(std::clamp(samples[i]*release*onset,-1.f,1.f)*32767.f);
  }
  return true;
}
}
