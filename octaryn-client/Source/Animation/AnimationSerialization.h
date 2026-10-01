#pragma once
#include "CookedAsset.h"
#include <span>

namespace octaryn::client::animation {
std::vector<std::uint8_t> encode_animation_payload(const CookedAsset&);
void decode_animation_payload(std::span<const std::uint8_t>,CookedAsset&);
}
