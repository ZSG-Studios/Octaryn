#include "GameUiState.h"

namespace octaryn::client::app {
void GameUi::set_audio_feedback(audio::ActionAudio* audio) {state_->audio_feedback.set_audio(audio);}
}
