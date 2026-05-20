#pragma once

#include "PlayerModelData.h"

#include <ozz/base/maths/simd_math.h>

#include <vector>

namespace octaryn_client_app {

bool initialize_player_model_animation(player_model_asset &asset);

void release_player_model_animation(player_model_asset &asset);

bool sample_player_model_animation(player_model_asset &asset,
                                   const char *animation_name, float seconds,
                                   std::vector<ozz::math::Float4x4> &skin);
bool sample_player_model_animation_transition(
    player_model_asset &asset, const char *animation_name, float seconds,
    const char *previous_animation_name, float previous_seconds,
    float previous_weight, std::vector<ozz::math::Float4x4> &skin);

} // namespace octaryn_client_app
