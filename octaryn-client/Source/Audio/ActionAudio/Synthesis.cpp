#include "ActionAudio.h"
#include <miniaudio.h>
#include <algorithm>
#include <cmath>

namespace octaryn::client::audio {
bool valid_action_sound(const SoundDefinition& definition) {
  return std::isfinite(definition.frequency) && definition.frequency>0 &&
      definition.frequency<ActionSampleRate/2 && std::isfinite(definition.gain) &&
      definition.gain>=0 && definition.gain<=4;
}
bool synthesize_action_sound(const SoundDefinition& definition,ActionSamples& output) {
  output.fill(0);
  if(!valid_action_sound(definition)) return false;
  std::array<float,ActionSampleCount> samples{};
  const auto config=ma_waveform_config_init(ma_format_f32,1,ActionSampleRate,
      ma_waveform_type_sine,definition.gain,definition.frequency);
  ma_waveform waveform{};
  if(ma_waveform_init(&config,&waveform)!=MA_SUCCESS) return false;
  ma_uint64 count{};
  const auto result=ma_waveform_read_pcm_frames(&waveform,samples.data(),samples.size(),&count);
  ma_waveform_uninit(&waveform);
  if(result!=MA_SUCCESS || count!=samples.size()) return false;
  // Original app/audio/synthesis.cpp quadratic fade and signed PCM conversion.
  for(std::size_t i=0;i<samples.size();++i) {
    const float fade=1-static_cast<float>(i)/static_cast<float>(samples.size());
    output[i]=static_cast<std::int16_t>(std::clamp(samples[i]*(fade*fade),-1.f,1.f)*32767.f);
  }
  return true;
}
}
