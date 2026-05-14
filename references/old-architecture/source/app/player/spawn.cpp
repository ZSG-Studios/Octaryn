#include <SDL3/SDL.h>

#include <cmath>

#include "world/block/block.h"
#include "core/camera/camera.h"
#include "core/persistence/persistence.h"
#include "internal.h"

namespace {

constexpr Uint64 kPlayerPersistIntervalNs = 1ull * SDL_NS_PER_SECOND;
constexpr float kPlayerPositionPersistEpsilon = 0.01f;
constexpr float kPlayerAnglePersistEpsilon = 0.001f;

typedef struct player_persist_data
{
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    block_t block;
}
player_persist_data_t;

typedef struct player_persist_cache
{
    player_persist_data_t data = {};
    int id = -1;
    Uint64 last_save_ticks = 0;
    bool valid = false;
}
player_persist_cache_t;

player_persist_cache_t g_player_persist_cache = {};

auto build_player_persist_data(const player_t* player) -> player_persist_data_t
{
    return {
        .x = player->camera.position[0],
        .y = player->camera.position[1],
        .z = player->camera.position[2],
        .pitch = player->camera.pitch,
        .yaw = player->camera.yaw,
        .block = player->block,
    };
}

void note_player_persisted(int id, const player_persist_data_t& data, Uint64 now_ticks)
{
    g_player_persist_cache.data = data;
    g_player_persist_cache.id = id;
    g_player_persist_cache.last_save_ticks = now_ticks;
    g_player_persist_cache.valid = true;
}

auto player_persist_data_matches_cache(int id, const player_persist_data_t& data) -> bool
{
    if (!g_player_persist_cache.valid || g_player_persist_cache.id != id)
    {
        return false;
    }

    return SDL_fabsf(data.x - g_player_persist_cache.data.x) <= kPlayerPositionPersistEpsilon &&
           SDL_fabsf(data.y - g_player_persist_cache.data.y) <= kPlayerPositionPersistEpsilon &&
           SDL_fabsf(data.z - g_player_persist_cache.data.z) <= kPlayerPositionPersistEpsilon &&
           SDL_fabsf(data.pitch - g_player_persist_cache.data.pitch) <= kPlayerAnglePersistEpsilon &&
           SDL_fabsf(data.yaw - g_player_persist_cache.data.yaw) <= kPlayerAnglePersistEpsilon &&
           data.block == g_player_persist_cache.data.block;
}

} // namespace

void player_apply_default_spawn(player_t* player)
{
    player->camera.position[0] = 0.0f;
    player->camera.position[1] = 80.0f;
    player->camera.position[2] = 0.0f;
    player->camera.pitch = -0.35f;
    player->camera.yaw = 0.0f;
    player->controller = PLAYER_CONTROLLER_WALK;
    player->velocity[0] = 0.0f;
    player->velocity[1] = 0.0f;
    player->velocity[2] = 0.0f;
    player->is_on_ground = false;
}

bool player_save_or_load(player_t* player, int id, bool save)
{
    player_persist_data_t data = {};

    if (save)
    {
        data = build_player_persist_data(player);
        persistence_set_player(id, &data, sizeof(data));
        note_player_persisted(id, data, SDL_GetTicksNS());
        return false;
    }

    camera_init(&player->camera, CAMERA_TYPE_PERSPECTIVE);
    player_apply_default_spawn(player);
    player->block = BLOCK_YELLOW_TORCH;
    if (persistence_get_player(id, &data, sizeof(data)))
    {
        if (std::isfinite(data.x) && std::isfinite(data.y) && std::isfinite(data.z) &&
            std::isfinite(data.pitch) && std::isfinite(data.yaw))
        {
            player->block = data.block;
            player->camera.position[0] = data.x;
            player->camera.position[1] = SDL_clamp(data.y, -1024.0f, 1024.0f);
            player->camera.position[2] = data.z;
            player->camera.pitch = SDL_clamp(data.pitch, -1.55f, 1.55f);
            player->camera.yaw = data.yaw;
            player->query = {};
            note_player_persisted(id, data, SDL_GetTicksNS());
            return true;
        }
    }

    player->query = {};
    return false;
}

void player_save_if_due(player_t* player, int id)
{
    if (!player)
    {
        return;
    }

    const player_persist_data_t data = build_player_persist_data(player);
    if (player_persist_data_matches_cache(id, data))
    {
        return;
    }

    const Uint64 now = SDL_GetTicksNS();
    if (g_player_persist_cache.valid && g_player_persist_cache.id == id &&
        now - g_player_persist_cache.last_save_ticks < kPlayerPersistIntervalNs)
    {
        return;
    }

    persistence_set_player(id, &data, sizeof(data));
    note_player_persisted(id, data, now);
}

void player_reset_spawn(player_t* player)
{
    player_apply_default_spawn(player);
}
