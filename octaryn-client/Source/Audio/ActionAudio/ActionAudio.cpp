#include "ActionAudio.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <limits>
#include <thread>

namespace octaryn::client::audio {
struct ActionAudio {
  ALCdevice* device{};
  ALCcontext* context{};
  std::array<ALuint,4> buffers{};
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
  if(!audio->checked()) return audio.release();
  for(std::size_t i=0;i<definitions.size();++i) {
    ActionSamples samples{};
    if(!synthesize_action_sound(definitions[i],samples)) {audio->message="audio_synthesis_failed";return audio.release();}
    alBufferData(audio->buffers[i],AL_FORMAT_MONO16,samples.data(),static_cast<ALsizei>(sizeof(samples)),ActionSampleRate);
    if(!audio->checked()) return audio.release();
  }
  audio->ready=true;audio->message=mode==OutputMode::Loopback?"ready_loopback":"ready_default_device";
  return audio.release();
}
void destroy_action_audio(ActionAudio* audio) {delete audio;}
PlayResult play_action_audio(ActionAudio* audio,ActionSound event) {
  const auto index=static_cast<std::size_t>(event);
  if(index>=4) return PlayResult::Invalid;
  if(!audio || !audio->current()) return PlayResult::Unavailable;
  for(const auto source:audio->sources) {
    ALint state{};alGetSourcei(source,AL_SOURCE_STATE,&state);
    if(!audio->checked()) return PlayResult::Unavailable;
    if(state==AL_PLAYING) continue;
    alSourceStop(source);alSourcei(source,AL_BUFFER,static_cast<ALint>(audio->buffers[index]));
    alSourcef(source,AL_GAIN,1);alSourcePlay(source);
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
bool render_action_audio_loopback(ActionAudio* audio,std::span<std::int16_t> samples) {
  if(!audio || !audio->render || !audio->current() || samples.empty() ||
      samples.size()>static_cast<std::size_t>(std::numeric_limits<ALCsizei>::max())) return false;
  audio->render(audio->device,samples.data(),static_cast<ALCsizei>(samples.size()));
  return audio->checked();
}
}
