#include "app/managed/managed_frame_snapshot.h"

#include "app/player/player.h"
#include "core/world_time/time.h"
#include "world/runtime/world.h"

#include <SDL3/SDL.h>

#include <cstdint>

namespace {

float key_axis(const bool* keys, SDL_Scancode positive, SDL_Scancode negative)
{
    return static_cast<float>(keys[positive]) - static_cast<float>(keys[negative]);
}

std::uint32_t input_flags(const player_t* player, const bool* keys, Uint32 mouse_buttons)
{
    std::uint32_t flags = 0u;
    if (keys[SDL_SCANCODE_SPACE])
    {
        flags |= OCT_MANAGED_INPUT_FLAG_JUMP;
    }
    if (keys[SDL_SCANCODE_LCTRL])
    {
        flags |= OCT_MANAGED_INPUT_FLAG_SPRINT;
    }
    if (player && player->controller == PLAYER_CONTROLLER_FLY)
    {
        flags |= OCT_MANAGED_INPUT_FLAG_FLY_MODE;
    }
    if ((mouse_buttons & SDL_BUTTON_LMASK) != 0u)
    {
        flags |= OCT_MANAGED_INPUT_FLAG_PRIMARY;
    }
    if ((mouse_buttons & SDL_BUTTON_RMASK) != 0u)
    {
        flags |= OCT_MANAGED_INPUT_FLAG_SECONDARY;
    }
    return flags;
}

} // namespace

oct_managed_frame_snapshot_t app_managed_frame_snapshot_build(const player_t* player,
                                                              const world_time_snapshot_t* world_time,
                                                              double delta_seconds,
                                                              std::uint64_t frame_index)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const Uint32 mouse_buttons = SDL_GetMouseState(nullptr, nullptr);

    world_debug_stats_t world_stats = {};
    world_get_debug_stats(&world_stats);

    oct_managed_frame_snapshot_t snapshot = {};
    snapshot.version = OCT_MANAGED_FRAME_SNAPSHOT_VERSION;
    snapshot.size = static_cast<std::uint32_t>(sizeof(snapshot));

    snapshot.input.version = OCT_MANAGED_FRAME_SNAPSHOT_VERSION;
    snapshot.input.size = static_cast<std::uint32_t>(sizeof(snapshot.input));
    snapshot.input.flags = input_flags(player, keys, mouse_buttons);
    snapshot.input.controller = player ? static_cast<std::uint32_t>(player->controller) : 0u;
    snapshot.input.move_x = key_axis(keys, SDL_SCANCODE_D, SDL_SCANCODE_A);
    snapshot.input.move_y = key_axis(keys, SDL_SCANCODE_E, SDL_SCANCODE_Q);
    snapshot.input.move_z = key_axis(keys, SDL_SCANCODE_W, SDL_SCANCODE_S);
    if (player)
    {
        snapshot.input.camera_x = player->camera.position[0];
        snapshot.input.camera_y = player->camera.position[1];
        snapshot.input.camera_z = player->camera.position[2];
        snapshot.input.camera_pitch = player->camera.pitch;
        snapshot.input.camera_yaw = player->camera.yaw;
    }
    SDL_Window* focus_window = SDL_GetKeyboardFocus();
    snapshot.input.relative_mouse =
        focus_window && SDL_GetWindowRelativeMouseMode(focus_window) ? 1 : 0;

    snapshot.world.version = OCT_MANAGED_FRAME_SNAPSHOT_VERSION;
    snapshot.world.size = static_cast<std::uint32_t>(sizeof(snapshot.world));
    snapshot.world.frame_index = frame_index;
    snapshot.world.delta_seconds = delta_seconds;
    snapshot.world.render_distance = world_get_render_distance();
    snapshot.world.active_chunks = world_stats.active_chunks;
    snapshot.world.loaded_chunks = world_stats.loaded_chunks;
    snapshot.world.mesh_ready_chunks = world_stats.mesh_ready_chunks;
    snapshot.world.running_jobs = world_stats.running_jobs;
    snapshot.world.pending_window_transition = world_has_pending_window_transition() ? 1 : 0;
    if (player)
    {
        snapshot.world.player_block_x = static_cast<std::int32_t>(SDL_floorf(player->camera.position[0]));
        snapshot.world.player_block_y = static_cast<std::int32_t>(SDL_floorf(player->camera.position[1]));
        snapshot.world.player_block_z = static_cast<std::int32_t>(SDL_floorf(player->camera.position[2]));
    }
    if (world_time)
    {
        snapshot.world.day_index = world_time->day_index;
        snapshot.world.total_world_seconds = world_time->total_world_seconds;
        snapshot.world.second_of_day = world_time->second_of_day;
    }

    return snapshot;
}
