#pragma once
#include "ActionAudio.h"
#include "../../Diagnostics/AsyncProfileStream.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace octaryn::client::app {
class AudioWorkload {
  audio::ActionAudio* audio_{};
  diagnostics::AsyncProfileStream profile_;
  std::array<std::int16_t,512> samples_{};
  double remainder_{};
  bool requested_{},started_{};
public:
  ~AudioWorkload() {if(started_)audio::stop_action_audio(audio_);}
  AudioWorkload(bool hidden,audio::ActionAudio* audio):audio_(hidden?audio:nullptr) {
    const char* value=SDL_getenv("OCTARYN_CLIENT_AUDIO_WORKLOAD");
    if(value && *value) {
      if(!hidden || std::string_view(value)!="8" || !audio_)
        throw std::runtime_error("Audio workload requires hidden loopback and eight voices");
      requested_=true;
      const char* path=SDL_getenv("OCTARYN_CLIENT_AUDIO_PROFILE");
      if(!path || !*path)throw std::runtime_error("Audio workload requires profile evidence");
      profile_.open(path);profile_<<"frame,active_voices,mixed_samples,cpu_ms,played,dropped\n";
      if(!profile_)throw std::runtime_error("Cannot open audio workload evidence");
    }
  }
  bool step(std::uint64_t frame,double seconds,bool ready) {
    if(!audio_ || !ready)return true;
    if(!std::isfinite(seconds) || seconds<0)return false;
    const auto begin=SDL_GetTicksNS();
    if(requested_ && !started_) {
      for(unsigned i=0;i<audio::ActionVoiceCount;++i)
        if(audio::play_action_audio(audio_,static_cast<audio::ActionSound>(i%4),true)!=audio::PlayResult::Played)return false;
      started_=true;
      std::puts("audio_workload voices=8 output=loopback continuous=1");
    }
    // A long stalled frame remains a qualification failure, not an unbounded mixer catch-up.
    if(seconds>1)return false;
    remainder_+=seconds*audio::ActionSampleRate;
    auto remaining=static_cast<unsigned>(remainder_);remainder_-=remaining;
    const auto mixed=remaining;
    while(remaining) {
      const auto count=std::min(remaining,static_cast<unsigned>(samples_.size()));
      if(!audio::render_action_audio_loopback(audio_,std::span(samples_.data(),count)))return false;
      remaining-=count;
    }
    if(requested_) {
      const auto status=audio::action_audio_status(audio_);
      profile_<<frame<<','<<status.active_voices<<','<<mixed<<','<<double(SDL_GetTicksNS()-begin)/1e6
              <<','<<status.played<<','<<status.dropped<<'\n';profile_.flush();
      if(!profile_ || !status.available || status.active_voices!=8)return false;
    }
    return true;
  }
};
}
