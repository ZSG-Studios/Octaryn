#include "ActionAudio.h"
#include "ActionSounds.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <vector>

void check_action_sound_definitions(const std::filesystem::path&,const std::filesystem::path&);
namespace {
using namespace octaryn::client::audio;
unsigned checks{};
// Independent oracle of the recovered basegame content, not playback defaults.
constexpr SoundDefinitions original{{{540,.18},{180,.35},{760,.12},{620,.10}}};
void require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
void cpu() {
  for(const auto& definition:original) {
    ActionSamples samples{},repeat{};
    require(synthesize_action_sound(definition,samples),"original waveform generation");
    require(synthesize_action_sound(definition,repeat) && samples==repeat,"deterministic PCM");
    int max_error{};
    for(std::size_t i=0;i<samples.size();++i) {
      const float sine=static_cast<float>(std::sin(2*std::numbers::pi*definition.frequency*
          static_cast<double>(i)/ActionSampleRate)*definition.gain);
      const float fade=1-static_cast<float>(i)/static_cast<float>(samples.size());
      const auto expected=static_cast<std::int16_t>(std::clamp(sine*(fade*fade),-1.f,1.f)*32767.f);
      max_error=std::max(max_error,std::abs(static_cast<int>(samples[i])-expected));
    }
    require(max_error<=1,"PCM differs from original sine/envelope/conversion");
    require(samples.front()==0 && std::abs(samples.back())<=1,"waveform endpoints fade to zero");
  }
  ActionSamples samples{};
  require(synthesize_action_sound({1000,2},samples),"bounded high gain synthesis");
  require(*std::max_element(samples.begin(),samples.end())==32767 &&
      *std::min_element(samples.begin(),samples.end())==-32767,"PCM saturation does not overflow");
  require(synthesize_action_sound({540,0},samples) &&
      std::all_of(samples.begin(),samples.end(),[](auto value){return value==0;}),"zero gain is silent");
  const SoundDefinition invalid[]={{0,.1},{24000,.1},{-1,.1},{540,-1},{540,4.1},
      {std::numeric_limits<double>::infinity(),.1},{540,std::numeric_limits<double>::quiet_NaN()}};
  for(const auto& definition:invalid) {
    samples.fill(123);
    require(!synthesize_action_sound(definition,samples) &&
        std::all_of(samples.begin(),samples.end(),[](auto value){return value==0;}),"invalid definition rejected with cleared PCM");
    auto definitions=original;definitions[0]=definition;
    ActionAudioOwner audio(create_action_audio(definitions,OutputMode::Loopback));
    const auto status=action_audio_status(audio.get());
    require(!status.available && std::string_view(status.message)=="invalid_sound_definition",
        "invalid configuration fails before any audio device operation");
    require(play_action_audio(audio.get(),ActionSound::Place)==PlayResult::Unavailable,"unavailable owner play");
  }
  destroy_action_audio(nullptr);
  require(!action_audio_status(nullptr).available,"null owner status");
  require(play_action_audio(nullptr,static_cast<ActionSound>(99))==PlayResult::Invalid,"invalid event rejected");
}
int loopback(const SoundDefinitions& definitions) {
  ActionAudioOwner audio(create_action_audio(definitions,OutputMode::Loopback));
  auto status=action_audio_status(audio.get());
  if(!status.available) {
    const std::string_view reason=status.message;
    const bool unavailable=reason=="audio_loopback_unavailable" || reason=="audio_loopback_api_unavailable" ||
        reason=="audio_loopback_format_unavailable";
    std::printf("action_audio_loopback=%s reason=%s\n",unavailable?"unavailable":"failed",status.message);
    return unavailable?77:1;
  }
  std::vector<std::int16_t> output(8000);
  std::array<std::uint64_t,4> signatures{};
  for(std::size_t sound=0;sound<original.size();++sound) {
    require(play_action_audio(audio.get(),static_cast<ActionSound>(sound))==PlayResult::Played,"loopback event start");
    require(action_audio_status(audio.get()).active_voices==1,"source enters playing state");
    require(render_action_audio_loopback(audio.get(),std::span(output).first(100)),"loopback partial advancement");
    require(action_audio_status(audio.get()).active_voices==1,"source remains active before duration");
    require(render_action_audio_loopback(audio.get(),output),"loopback PCM render");
    std::uint64_t energy{};
    for(const auto value:output) energy+=static_cast<std::uint64_t>(std::abs(static_cast<int>(value)));
    require(energy>10000,"loopback event must produce nonzero PCM");
    signatures[sound]=energy;
    status=action_audio_status(audio.get());
    require(status.available && status.active_voices==0,"source completes without backend error");
  }
  for(std::size_t i=0;i<4;++i) for(std::size_t j=i+1;j<4;++j)
    require(signatures[i]!=signatures[j],"four sounds retain distinct output");
  for(std::size_t i=0;i<ActionVoiceCount;++i)
    require(play_action_audio(audio.get(),ActionSound::Change)==PlayResult::Played,"eight available voices");
  require(play_action_audio(audio.get(),ActionSound::Change)==PlayResult::Busy,"ninth voice drops without stealing or queueing");
  status=action_audio_status(audio.get());
  require(status.available && status.active_voices==8 && status.played==12 && status.dropped==1,"bounded pool accounting");
  require(render_action_audio_loopback(audio.get(),output),"pool completion render");
  require(action_audio_status(audio.get()).active_voices==0,"all voices complete");
  require(play_action_audio(audio.get(),ActionSound::Place)==PlayResult::Played,"completed voice reused");
  audio.reset(); // Destruction while a source is playing must be safe.
  audio.reset(create_action_audio(definitions,OutputMode::Loopback));
  require(action_audio_status(audio.get()).available,"loopback reinitialization after active-source teardown");
  require(play_action_audio(audio.get(),ActionSound::Break)==PlayResult::Played &&
      render_action_audio_loopback(audio.get(),output),"reinitialized backend playback");
  std::printf("action_audio_loopback=passed checks=%u backend=OpenAL_SOFT_loopback speaker_devices=0 sounds=4 voices=8\n",checks);
  return 0;
}
}
int main(int argc,char** argv) {
  try {
    if(argc!=4 || (std::string_view(argv[1])!="--cpu" && std::string_view(argv[1])!="--loopback")) {
      std::fprintf(stderr,"usage: action_audio_probe --cpu|--loopback catalog scratch\n");return 2;
    }
    check_action_sound_definitions(argv[2],argv[3]);
    if(std::string_view(argv[1])=="--loopback") return loopback(octaryn::client::app::load_action_sounds(argv[2]));
    cpu();
    std::printf("action_audio_cpu=passed checks=%u sounds=4 samples_per_sound=4000 devices=0\n",checks);
    return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"action_audio_probe=failed checks=%u reason=%s\n",checks,error.what());return 1;
  }
}
