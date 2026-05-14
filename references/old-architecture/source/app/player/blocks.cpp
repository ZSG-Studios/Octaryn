#include <SDL3/SDL.h>

#include "app/audio/audio.h"
#include "world/block/block.h"
#include "world/runtime/world.h"
#include "internal.h"

void player_place_block(player_t* player)
{
    if (player->query.block == BLOCK_EMPTY)
    {
        return;
    }

    if (!block_is_solid(player->block))
    {
        if (world_queue_block_edit(player->query.previous, player->block))
        {
            player_update_query(player);
            oct_audio_play_event(OCT_AUDIO_EVENT_PLACE);
        }
        return;
    }

    const aabb_t aabb = player_get_aabb();
    for (int i = 0; i < 3; i++)
    {
        float min = player->camera.position[i] + aabb.min[i] + PLAYER_PHYSICS_EPSILON;
        float max = player->camera.position[i] + aabb.max[i] - PLAYER_PHYSICS_EPSILON;
        if (max <= static_cast<float>(player->query.previous[i]) ||
            min >= static_cast<float>(player->query.previous[i]) + 1.0f)
        {
            if (world_queue_block_edit(player->query.previous, player->block))
            {
                player_update_query(player);
                oct_audio_play_event(OCT_AUDIO_EVENT_PLACE);
            }
            break;
        }
    }
}

void player_select_block(player_t* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        player->block = player->query.block;
        oct_audio_play_event(OCT_AUDIO_EVENT_SELECT);
    }
}

void player_break_block(player_t* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        if (world_queue_block_edit(player->query.current, BLOCK_EMPTY))
        {
            player_update_query(player);
            oct_audio_play_event(OCT_AUDIO_EVENT_BREAK);
        }
    }
}

void player_change_block(player_t* player, int dy)
{
    do
    {
        int block = player->block - (BLOCK_EMPTY + 1) + dy;
        int count = BLOCK_COUNT - BLOCK_EMPTY - 1;
        block = (block + count) % count;
        player->block = static_cast<block_t>(block + BLOCK_EMPTY + 1);
    }
    while (!block_is_placeable(player->block));

    oct_audio_play_event(OCT_AUDIO_EVENT_CHANGE);
}
