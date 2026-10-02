#pragma once
#include "ActionAudio.h"
#include <cmath>

namespace octaryn::client::audio {
// v1 action IDs are the four declared built-ins, never arbitrary clip hashes.
inline bool action_sound_request(std::uint64_t id,float volume,float x,float y,float z,ActionSound& sound) {
  if(id>3 || !std::isfinite(volume) || volume<0 || volume>1 ||
      !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))return false;
  sound=static_cast<ActionSound>(id);return true;
}
}
