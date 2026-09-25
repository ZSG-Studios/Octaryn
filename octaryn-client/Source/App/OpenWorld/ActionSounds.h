#pragma once
#include "ActionAudio.h"
#include <filesystem>

namespace octaryn::client::app {
audio::SoundDefinitions load_action_sounds(const std::filesystem::path& path);
}
