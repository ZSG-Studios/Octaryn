#pragma once
#include "ActionAudio.h"
#include <filesystem>

namespace octaryn::client::app {

// Loads an optional game-provided action sound catalog. A missing catalog is
// not an error: the engine runs silently until a game ships one.
audio::SoundDefinitions load_action_sounds(const std::filesystem::path& path, bool& present);

}
