#include "ActionAudio.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <cmath>
#include <limits>
#include <thread>
#include <unordered_map>

namespace octaryn::client::audio {
struct ActionAudio {
  struct Clip {ALuint buffer{};std::size_t bytes{};std::uint32_t channels{};};
  std::unordered_map<std::uint64_t,Clip> clips;
  std::array<ALuint,32> pcm_sources{};
  std::array<std::uint64_t,32> pcm_voices{},pcm_clips{};
  std::uint64_t next_clip{1},next_voice{1};std::size_t pcm_bytes{};
  ALCdevice* device{};
  ALCcontext* context{};
  std::array<ALuint,ActionSoundCount> buffers{};
  std::array<ALuint,ActionVoiceCount> sources{};
  LPALCRENDERSAMPLESSOFT render{};
  std::thread::id owner{std::this_thread::get_id()};
  bool ready{},disconnect_supported{};
  std::uint64_t played{},dropped{};
  const char* message{"initializing"};
  ~ActionAudio() {
    // One cleanup owner, including partial initialization; no recursive mutex.
    if(context) {
      if(alcMakeContextCurrent(context)) {
        for(auto source:pcm_sources) if(source) alDeleteSources(1,&source);
        for(const auto& entry:clips)alDeleteBuffers(1,&entry.second.buffer);
        for(auto source:sources) if(source) alDeleteSources(1,&source);
        for(auto buffer:buffers) if(buffer) alDeleteBuffers(1,&buffer);
        alcMakeContextCurrent(nullptr);
      }
      alcDestroyContext(context);
    }
    if(device) alcCloseDevice(device);
  }
  bool connected() {
    if(!disconnect_supported) return true;
    ALCint connected{};
    alcGetIntegerv(device,ALC_CONNECTED,1,&connected);
    if(alcGetError(device)!=ALC_NO_ERROR) {ready=false;message="audio_backend_error";return false;}
    if(connected==ALC_FALSE) {ready=false;message="audio_device_disconnected";return false;}
    return true;
  }
  bool current() {
    if(!ready) return false;
    if(owner!=std::this_thread::get_id()) {message="audio_thread_mismatch";return false;}
    if(!connected()) return false;
    if(!alcMakeContextCurrent(context)) {ready=false;message="audio_context_unavailable";return false;}
    return true;
  }
  bool checked() {
    // Recheck after alSourcePlay: OpenAL may silently stop a disconnected source.
    if(!connected()) return false;
    if(alGetError()==AL_NO_ERROR && alcGetError(device)==ALC_NO_ERROR) return true;
    ready=false;message="audio_backend_error";return false;
  }
};
ActionAudio* create_action_audio(const SoundDefinitions& definitions,OutputMode mode) {
  ActionAudioOwner audio(new ActionAudio);
  for(const auto& definition:definitions) if(!valid_action_sound(definition)) {
    audio->message="invalid_sound_definition";return audio.release();
  }
  if(mode==OutputMode::Loopback) {
    if(!alcIsExtensionPresent(nullptr,"ALC_SOFT_loopback")) {
      audio->message="audio_loopback_unavailable";return audio.release();
    }
    const auto open=reinterpret_cast<LPALCLOOPBACKOPENDEVICESOFT>(alcGetProcAddress(nullptr,"alcLoopbackOpenDeviceSOFT"));
    const auto supported=reinterpret_cast<LPALCISRENDERFORMATSUPPORTEDSOFT>(alcGetProcAddress(nullptr,"alcIsRenderFormatSupportedSOFT"));
    audio->render=reinterpret_cast<LPALCRENDERSAMPLESSOFT>(alcGetProcAddress(nullptr,"alcRenderSamplesSOFT"));
    if(!open || !supported || !audio->render) {audio->message="audio_loopback_api_unavailable";return audio.release();}
    audio->device=open(nullptr);
    if(!audio->device || !supported(audio->device,ActionSampleRate,ALC_MONO_SOFT,ALC_SHORT_SOFT)) {
      audio->message="audio_loopback_format_unavailable";return audio.release();
    }
  } else if(mode==OutputMode::DefaultDevice) audio->device=alcOpenDevice(nullptr);
  else {audio->message="invalid_output_mode";return audio.release();}
  if(!audio->device) {audio->message="audio_device_unavailable";return audio.release();}
  audio->disconnect_supported=alcIsExtensionPresent(audio->device,"ALC_EXT_disconnect")==ALC_TRUE;
  const ALCint attributes[]={ALC_FREQUENCY,ActionSampleRate,ALC_FORMAT_CHANNELS_SOFT,ALC_MONO_SOFT,
      ALC_FORMAT_TYPE_SOFT,ALC_SHORT_SOFT,0};
  audio->context=alcCreateContext(audio->device,mode==OutputMode::Loopback?attributes:nullptr);
  if(!audio->context || !alcMakeContextCurrent(audio->context)) {
    audio->message="audio_context_unavailable";return audio.release();
  }
  alGenBuffers(static_cast<ALsizei>(audio->buffers.size()),audio->buffers.data());
  alGenSources(static_cast<ALsizei>(audio->sources.size()),audio->sources.data());
  alGenSources(static_cast<ALsizei>(audio->pcm_sources.size()),audio->pcm_sources.data());
  if(!audio->checked()) return audio.release();
  for(std::size_t i=0;i<definitions.size();++i) {
    ActionSamples samples{};
    if(!synthesize_action_sound(definitions[i],samples)) {audio->message="audio_synthesis_failed";return audio.release();}
    const auto frames=static_cast<ALsizei>(std::lround(definitions[i].duration_ms*ActionSampleRate/1000));
    const auto bytes=frames*static_cast<ALsizei>(sizeof(samples[0]));
    alBufferData(audio->buffers[i],AL_FORMAT_MONO16,samples.data(),bytes,ActionSampleRate);
    if(!audio->checked()) return audio.release();
  }
  audio->ready=true;audio->message=mode==OutputMode::Loopback?"ready_loopback":"ready_default_device";
  return audio.release();
}
void destroy_action_audio(ActionAudio* audio) {delete audio;}
PlayResult play_action_audio(ActionAudio* audio,ActionSound event,bool loop,float volume) {
  const auto index=static_cast<std::size_t>(event);
  if(index>=ActionSoundCount || !std::isfinite(volume) || volume<0 || volume>1) return PlayResult::Invalid;
  if(!audio || !audio->current()) return PlayResult::Unavailable;
  for(const auto source:audio->sources) {
    ALint state{};alGetSourcei(source,AL_SOURCE_STATE,&state);
    if(!audio->checked()) return PlayResult::Unavailable;
    if(state==AL_PLAYING) continue;
    alSourceStop(source);alSourcei(source,AL_BUFFER,static_cast<ALint>(audio->buffers[index]));
    alSourcei(source,AL_LOOPING,loop?AL_TRUE:AL_FALSE);
    alSourcef(source,AL_GAIN,volume);alSourcePlay(source);
    if(!audio->checked()) return PlayResult::Unavailable;
    ++audio->played;return PlayResult::Played;
  }
  ++audio->dropped;return PlayResult::Busy;
}
ActionAudioStatus action_audio_status(ActionAudio* audio) {
  if(!audio) return {false,0,0,0,"audio_missing"};
  std::uint32_t active{};
  const bool current=audio->current();
  if(current) for(const auto source:audio->sources) {
    ALint state{};alGetSourcei(source,AL_SOURCE_STATE,&state);
    if(state==AL_PLAYING) ++active;
  }
  const bool available=current && audio->checked();
  return {available,active,audio->played,audio->dropped,audio->message};
}
bool stop_action_audio(ActionAudio* audio) {
  if(!audio || !audio->current())return false;
  for(const auto source:audio->sources) {alSourceStop(source);alSourcei(source,AL_LOOPING,AL_FALSE);}
  return audio->checked();
}
bool render_action_audio_loopback(ActionAudio* audio,std::span<std::int16_t> samples) {
  if(!audio || !audio->render || !audio->current() || samples.empty() ||
      samples.size()>static_cast<std::size_t>(std::numeric_limits<ALCsizei>::max())) return false;
  audio->render(audio->device,samples.data(),static_cast<ALCsizei>(samples.size()));
  return audio->checked();
}
bool register_pcm16(ActionAudio* a,std::span<const std::uint8_t> bytes,std::uint32_t rate,std::uint32_t channels,std::uint64_t& handle) {
 handle=0;if(bytes.empty() || bytes.size()>16*1024*1024 || (channels!=1 && channels!=2) ||
 rate<8000 || rate>192000 || bytes.size()%(channels*2)!=0 || !a || !a->current() ||
 a->clips.size()>=1024 || bytes.size()>128*1024*1024-a->pcm_bytes)return false;
 ALuint buffer{};alGenBuffers(1,&buffer);alBufferData(buffer,channels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16,
 bytes.data(),static_cast<ALsizei>(bytes.size()),static_cast<ALsizei>(rate));
 if(!a->checked()){if(buffer)alDeleteBuffers(1,&buffer);return false;}
 handle=a->next_clip++;a->clips.emplace(handle,ActionAudio::Clip{buffer,bytes.size(),channels});a->pcm_bytes+=bytes.size();return true;
}
PlayResult play_pcm_clip(ActionAudio* a,std::uint64_t clip,float gain,std::uint32_t flags,float x,float y,float z,std::uint64_t& voice) {
 voice=0;if(!std::isfinite(gain) || gain<0 || gain>1 || flags>3 || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))return PlayResult::Invalid;
 if(!a || !a->current())return PlayResult::Unavailable;
 const auto found=a->clips.find(clip);if(found==a->clips.end() || (!(flags&2) && found->second.channels!=1))return PlayResult::Invalid;
 for(std::size_t i=0;i<a->pcm_sources.size();++i){const auto source=a->pcm_sources[i];ALint state{};alGetSourcei(source,AL_SOURCE_STATE,&state);
 if(!a->checked())return PlayResult::Unavailable;if(state==AL_PLAYING)continue;
 alSourceStop(source);alSourcei(source,AL_BUFFER,static_cast<ALint>(found->second.buffer));alSourcei(source,AL_LOOPING,(flags&1)?AL_TRUE:AL_FALSE);
 alSourcei(source,AL_SOURCE_RELATIVE,(flags&2)?AL_TRUE:AL_FALSE);alSourcef(source,AL_ROLLOFF_FACTOR,0);
 alSource3f(source,AL_POSITION,(flags&2)?0:x,(flags&2)?0:y,(flags&2)?0:z);alSourcef(source,AL_GAIN,gain);alSourcePlay(source);
 if(!a->checked())return PlayResult::Unavailable;voice=a->next_voice++;a->pcm_voices[i]=voice;a->pcm_clips[i]=clip;++a->played;return PlayResult::Played;
 }++a->dropped;return PlayResult::Busy;
}
bool stop_pcm_voice(ActionAudio* a,std::uint64_t voice){
 if(!voice || !a || !a->current())return false;
 for(std::size_t i=0;i<a->pcm_voices.size();++i)if(a->pcm_voices[i]==voice){alSourceStop(a->pcm_sources[i]);alSourcei(a->pcm_sources[i],AL_BUFFER,0);a->pcm_voices[i]=0;a->pcm_clips[i]=0;return a->checked();}return false;
}
bool release_pcm_clip(ActionAudio* a,std::uint64_t clip){
 if(!clip || !a || !a->current())return false;const auto found=a->clips.find(clip);if(found==a->clips.end())return false;
 for(std::size_t i=0;i<a->pcm_clips.size();++i)if(a->pcm_clips[i]==clip){alSourceStop(a->pcm_sources[i]);alSourcei(a->pcm_sources[i],AL_BUFFER,0);a->pcm_voices[i]=0;a->pcm_clips[i]=0;}
 alDeleteBuffers(1,&found->second.buffer);if(!a->checked())return false;a->pcm_bytes-=found->second.bytes;a->clips.erase(found);return true;
}

bool query_pcm_voice(ActionAudio* a,std::uint64_t voice,bool& playing){
 playing=false;if(!voice || !a || !a->current())return false;
 for(std::size_t i=0;i<a->pcm_voices.size();++i)if(a->pcm_voices[i]==voice){ALint state{};alGetSourcei(a->pcm_sources[i],AL_SOURCE_STATE,&state);playing=state==AL_PLAYING;return a->checked();}return false;
}

bool clear_pcm_audio(ActionAudio* a){if(!a || !a->current())return false;while(!a->clips.empty())if(!release_pcm_clip(a,a->clips.begin()->first))return false;return true;}

}
