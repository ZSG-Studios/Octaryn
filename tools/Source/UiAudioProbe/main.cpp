#include "ActionAudio.h"
#include "ActionSounds.h"
#include <miniaudio.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace audio=octaryn::client::audio;
namespace app=octaryn::client::app;
namespace {
std::size_t checks{};
void require(bool condition,const char* message) {
  if(!condition) throw std::runtime_error(message);
  ++checks;
}
audio::ActionSamples original_sound(double frequency,double gain) {
  std::array<float,audio::ActionSampleCount> samples{};
  const auto config=ma_waveform_config_init(ma_format_f32,1,audio::ActionSampleRate,ma_waveform_type_sine,gain,frequency);
  ma_waveform waveform{};
  require(ma_waveform_init(&config,&waveform)==MA_SUCCESS,"reference waveform init");
  ma_uint64 count{};
  const auto result=ma_waveform_read_pcm_frames(&waveform,samples.data(),samples.size(),&count);
  ma_waveform_uninit(&waveform);
  require(result==MA_SUCCESS && count==samples.size(),"reference waveform read");
  audio::ActionSamples output{};
  for(std::size_t i=0;i<samples.size();++i) {
    const float fade=1-static_cast<float>(i)/static_cast<float>(samples.size());
    output[i]=static_cast<std::int16_t>(std::clamp(samples[i]*(fade*fade),-1.f,1.f)*32767.f);
  }
  return output;
}
double rms(const std::vector<std::int16_t>& samples) {
  double sum{};
  for(const auto sample:samples)sum+=static_cast<double>(sample)*sample;
  return std::sqrt(sum/static_cast<double>(samples.size()));
}
int peak(const std::vector<std::int16_t>& samples) {
  int result{};
  for(const auto sample:samples)result=std::max(result,std::abs(static_cast<int>(sample)));
  return result;
}
std::vector<std::int16_t> render(audio::ActionAudio* owner,std::size_t count) {
  std::vector<std::int16_t> samples(count);
  require(audio::render_action_audio_loopback(owner,samples),"loopback render");
  return samples;
}
void validate_synthesis(const audio::SoundDefinitions& definitions) {
  require(static_cast<int>(audio::ActionSound::Place)==0 && static_cast<int>(audio::ActionSound::Change)==3,
      "gameplay enum indices preserved");
  constexpr std::array frequencies{540.,180.,760.,620.};
  constexpr std::array gains{.18,.35,.12,.10};
  std::array<audio::ActionSamples,audio::ActionSoundCount> samples{};
  for(std::size_t i=0;i<definitions.size();++i) {
    require(audio::valid_action_sound(definitions[i]),"catalog definition valid");
    require(audio::synthesize_action_sound(definitions[i],samples[i]),"synthesis succeeds");
    if(i<4)require(samples[i]==original_sound(frequencies[i],gains[i]),"gameplay PCM preserved");
    else {
      const auto frames=static_cast<std::size_t>(std::lround(definitions[i].duration_ms*audio::ActionSampleRate/1000));
      require(definitions[i].gain<.1 && frames<2400,"UI tones short and subdued");
      require(samples[i][0]==0 && std::abs(static_cast<int>(samples[i][frames-1]))<=2,"UI envelope edges quiet");
      const auto end=samples[i].begin()+static_cast<std::ptrdiff_t>(frames);
      require(std::all_of(end,samples[i].end(),[](auto v){return v==0;}),"UI duration zero padding");
      require(std::any_of(samples[i].begin(),end,[](auto v){return std::abs(v)>100;}),"UI tone carries signal");
    }
    for(std::size_t j=0;j<i;++j) require(samples[i]!=samples[j],"each feedback waveform distinct");
  }
  auto invalid=definitions[4];
  invalid.duration_ms=84;
  require(!audio::valid_action_sound(invalid),"reject oversized duration");
  invalid=definitions[4];invalid.attack_ms=invalid.duration_ms;
  require(!audio::valid_action_sound(invalid),"reject invalid attack");
  invalid=definitions[4];invalid.fade_power=std::numeric_limits<double>::quiet_NaN();
  require(!audio::valid_action_sound(invalid),"reject nonfinite envelope");
  require(!audio::synthesize_action_sound(invalid,samples[0]) &&
      std::all_of(samples[0].begin(),samples[0].end(),[](auto v){return v==0;}),"invalid synthesis stays silent");
  auto invalid_catalog=definitions;invalid_catalog[4]=invalid;
  audio::ActionAudioOwner rejected(audio::create_action_audio(invalid_catalog,audio::OutputMode::Loopback));
  require(!audio::action_audio_status(rejected.get()).available,"invalid catalog cannot initialize backend");
}
void validate_loopback(const audio::SoundDefinitions& definitions) {
  audio::ActionAudioOwner owner(audio::create_action_audio(definitions,audio::OutputMode::Loopback));
  const auto initial=audio::action_audio_status(owner.get());
  require(initial.available,std::string(initial.message).c_str());
  require(peak(render(owner.get(),512))<=2,"idle loopback silent");
  std::array<double,audio::ActionSoundCount> energies{};
  for(std::size_t i=0;i<definitions.size();++i) {
    require(audio::play_action_audio(owner.get(),static_cast<audio::ActionSound>(i))==audio::PlayResult::Played,"play each action");
    const auto frames=static_cast<std::size_t>(std::lround(definitions[i].duration_ms*audio::ActionSampleRate/1000));
    const auto signal=render(owner.get(),frames+256);
    energies[i]=rms(signal);
    require(energies[i]>10 && peak(signal)<32767,"loopback signal without clipping");
    require(audio::action_audio_status(owner.get()).active_voices==0,"voice released at catalog duration");
    require(peak(render(owner.get(),512))<=2,"released voice silent");
  }
  for(std::size_t i=4;i<energies.size();++i)
    for(std::size_t j=4;j<i;++j)require(std::abs(energies[i]-energies[j])>1,"UI loopback energies distinct");
  require(audio::play_action_audio(owner.get(),audio::ActionSound::Count)==audio::PlayResult::Invalid,"invalid enum rejected");
  require(audio::play_action_audio(owner.get(),static_cast<audio::ActionSound>(-1))==audio::PlayResult::Invalid,"negative enum rejected");
  for(std::size_t i=0;i<audio::ActionVoiceCount;++i)
    require(audio::play_action_audio(owner.get(),audio::ActionSound::UiClick,true)==audio::PlayResult::Played,"bounded pool accepts voice");
  require(audio::play_action_audio(owner.get(),audio::ActionSound::UiHover)==audio::PlayResult::Busy,"pool refuses ninth voice");
  const auto busy=audio::action_audio_status(owner.get());
  require(busy.active_voices==audio::ActionVoiceCount && busy.dropped==1,"pool pressure accounted");
  require(peak(render(owner.get(),2048))<32767,"eight UI voices do not clip");
  require(audio::stop_action_audio(owner.get()),"stop voices");
  render(owner.get(),512);
  require(audio::action_audio_status(owner.get()).active_voices==0 && peak(render(owner.get(),512))<=2,"stop returns silent idle");
  require(audio::action_audio_status(owner.get()).played==audio::ActionSoundCount+audio::ActionVoiceCount,"play accounting exact");
}
void validate_catalog(const std::filesystem::path& source) {
  std::ifstream input(source,std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(input)),{});
  const auto scratch=std::filesystem::temp_directory_path()/
      ("octaryn-ui-audio-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
  struct RemoveFile {
    std::filesystem::path path;
    ~RemoveFile(){std::error_code error;std::filesystem::remove(path,error);}
  } cleanup{scratch};
  bool present=true;
  app::load_action_sounds(scratch,present);
  require(!present,"missing optional catalog stays silent");
  const auto reject=[&](const std::string& invalid,const char* message) {
    {std::ofstream output(scratch,std::ios::binary|std::ios::trunc);output<<invalid;}
    bool rejected{};
    try {app::load_action_sounds(scratch,present);}catch(const std::runtime_error&){rejected=true;}
    require(rejected && !present,message);
  };
  auto wrong_schema=text;
  const auto schema=wrong_schema.find("action-sounds.v2");
  require(schema!=std::string::npos,"current catalog schema");
  wrong_schema.replace(schema,16,"action-sounds.v1");
  reject(wrong_schema,"outdated catalog rejected");
  auto wrong_name=text;
  const auto name=wrong_name.find("ui_hover");
  require(name!=std::string::npos,"hover definition named");
  wrong_name.replace(name,8,"ui_other");
  reject(wrong_name,"unknown/missing action rejected");
  auto unknown_field=text;
  unknown_field.insert(unknown_field.find('{')+1,"\"unknown\":1,");
  reject(unknown_field,"unknown catalog fields rejected");
  reject(std::string(8193,' '),"oversized catalog rejected");
  reject("{","malformed catalog rejected");
}
}
int main(int argc,char** argv) {
  try {
    if(argc!=2)throw std::runtime_error("Usage: octaryn_ui_audio_probe <action-sounds.json>");
    bool present{};
    const auto definitions=app::load_action_sounds(argv[1],present);
    require(present,"catalog present");
    validate_catalog(argv[1]);
    validate_synthesis(definitions);
    validate_loopback(definitions);
    std::printf("{\"success\":true,\"checks\":%zu,\"sounds\":%zu,\"voices\":%zu,\"output\":\"silent_loopback\"}\n",
        checks,audio::ActionSoundCount,audio::ActionVoiceCount);
    return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"UI audio validation failed: %s\n",error.what());
    return 1;
  }
}
