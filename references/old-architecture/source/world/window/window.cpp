#include <SDL3/SDL.h>

#include "core/camera/camera.h"
#include "core/check.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"
#include "world/window/window.h"
#include "world/window/internal.h"

namespace {

constexpr Uint32 kInvalidChunkSlot = 0xFFFFFFFFu;

auto world_chunk_slot_id_for_index(int x, int z) -> Uint32
{
    return static_cast<Uint32>(x * MAX_WORLD_WIDTH + z);
}

void world_window_request_chunk(chunk_t* chunk, int world_x, int world_z)
{
    SDL_SetAtomicInt(&chunk->block_state, JOB_STATE_REQUESTED);
    SDL_SetAtomicInt(&chunk->mesh_state, JOB_STATE_REQUESTED);
    SDL_SetAtomicInt(&chunk->light_state, JOB_STATE_REQUESTED);
    SDL_SetAtomicInt(&chunk->light_dirty_flags, LIGHT_DIRTY_ALL);
    SDL_AddAtomicInt(&chunk->mesh_epoch, 1);
    SDL_AddAtomicInt(&chunk->light_epoch, 1);
    SDL_SetAtomicInt(&chunk->render_mesh_valid, 0);
    chunk->position[0] = world_x * CHUNK_WIDTH;
    chunk->position[1] = world_z * CHUNK_WIDTH;
    SDL_zeroa(chunk->pooled_face_offsets);
    SDL_zeroa(chunk->pooled_face_counts);
    chunk->pooled_skylight_offset = 0;
    chunk->pooled_skylight_count = 0;
    world_runtime_sync_chunk_descriptor_internal(chunk);
}

void world_window_place_chunk(chunk_t* chunk, int world_x, int world_z)
{
    chunk->position[0] = world_x * CHUNK_WIDTH;
    chunk->position[1] = world_z * CHUNK_WIDTH;
    world_runtime_sync_chunk_descriptor_internal(chunk);
}

bool world_window_chunk_can_preserve_overlap(chunk_t* chunk)
{
    return SDL_GetAtomicInt(&chunk->block_state) == JOB_STATE_COMPLETED;
}

} // namespace

auto world_window_origin_for_position_with_width(float position, int active_world_width) -> int
{
    return static_cast<int>(SDL_floorf(position / static_cast<float>(CHUNK_WIDTH))) - active_world_width / 2;
}

auto world_window_origin_for_position(float position) -> int
{
    return world_window_origin_for_position_with_width(position, world_active_world_width_internal());
}

static int sort_function(void* userdata, const void* lhs, const void* rhs)
{
    (void) userdata;
    const int center = world_active_world_width_internal() / 2;
    const int* left = static_cast<const int*>(lhs);
    const int* right = static_cast<const int*>(rhs);
    const int left_distance = (left[0] - center) * (left[0] - center) + (left[1] - center) * (left[1] - center);
    const int right_distance = (right[0] - center) * (right[0] - center) + (right[1] - center) * (right[1] - center);
    if (left_distance < right_distance)
    {
        return -1;
    }
    if (left_distance > right_distance)
    {
        return 1;
    }
    return 0;
}

void world_window_sort_chunks_internal(void)
{
    const int active_world_width = world_active_world_width_internal();
    int (*sorted_chunks)[2] = world_sorted_chunks_mutable_internal();
    int index = 0;
    for (int x = 0; x < active_world_width; ++x)
    for (int z = 0; z < active_world_width; ++z)
    {
        sorted_chunks[index][0] = x;
        sorted_chunks[index][1] = z;
        index++;
    }
    SDL_qsort_r(sorted_chunks, static_cast<size_t>(active_world_width * active_world_width), sizeof(int) * 2, sort_function, nullptr);
}

void world_window_reframe_internal(int origin_x, int origin_z, int active_world_width)
{
    const int previous_origin_x = world_origin_x_internal();
    const int previous_origin_z = world_origin_z_internal();
    const int previous_active_world_width = world_active_world_width_internal();
    chunk_t* translated_chunks[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH] = {};
    Uint32 translated_source_slots[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH] = {};
    chunk_t* spare_chunks[MAX_WORLD_WIDTH * MAX_WORLD_WIDTH] = {};
    int spare_count = 0;
    int translated_count = 0;

    for (int x = 0; x < active_world_width; ++x)
    for (int z = 0; z < active_world_width; ++z)
    {
        translated_source_slots[x][z] = kInvalidChunkSlot;
    }

    world_runtime_begin_pooled_chunk_slot_reframe_internal();

    for (int x = 0; x < previous_active_world_width; ++x)
    for (int z = 0; z < previous_active_world_width; ++z)
    {
        chunk_t* chunk = world_get_chunk_slot_internal(x, z);
        CHECK(chunk);

        const int world_x = previous_origin_x + x;
        const int world_z = previous_origin_z + z;
        const int translated_x = world_x - origin_x;
        const int translated_z = world_z - origin_z;
        if (translated_x >= 0 && translated_z >= 0 && translated_x < active_world_width && translated_z < active_world_width)
        {
            translated_chunks[translated_x][translated_z] = chunk;
            translated_source_slots[translated_x][translated_z] = world_chunk_slot_id_for_index(x, z);
            translated_count++;
        }
        else
        {
            spare_chunks[spare_count++] = chunk;
        }

        world_set_chunk_internal(x, z, nullptr);
    }

    const int needed_spares = active_world_width * active_world_width - translated_count;
    if (spare_count < needed_spares)
    {
        for (int x = 0; x < MAX_WORLD_WIDTH && spare_count < needed_spares; ++x)
        for (int z = 0; z < MAX_WORLD_WIDTH && spare_count < needed_spares; ++z)
        {
            if (x < previous_active_world_width && z < previous_active_world_width)
            {
                continue;
            }
            chunk_t* chunk = world_get_chunk_slot_internal(x, z);
            if (!chunk)
            {
                continue;
            }
            spare_chunks[spare_count++] = chunk;
            world_set_chunk_internal(x, z, nullptr);
        }
    }

    world_set_active_world_width_internal(active_world_width);
    world_set_origin_internal(origin_x, origin_z);

    for (int x = 0; x < active_world_width; ++x)
    for (int z = 0; z < active_world_width; ++z)
    {
        const Uint32 destination_slot = world_chunk_slot_id_for_index(x, z);
        chunk_t* chunk = nullptr;
        chunk = translated_chunks[x][z];
        if (!chunk)
        {
            CHECK(spare_count > 0);
            chunk = spare_chunks[--spare_count];
            world_runtime_clear_pooled_chunk_slot_internal(destination_slot);
            world_set_chunk_internal(x, z, chunk);
            world_window_request_chunk(chunk, origin_x + x, origin_z + z);
        }
        else
        {
            CHECK(translated_source_slots[x][z] != kInvalidChunkSlot);
            world_runtime_move_pooled_chunk_slot_internal(translated_source_slots[x][z], destination_slot);
            world_set_chunk_internal(x, z, chunk);
            world_window_place_chunk(chunk, origin_x + x, origin_z + z);
            if (!world_window_chunk_can_preserve_overlap(chunk))
            {
                world_window_request_chunk(chunk, origin_x + x, origin_z + z);
                world_runtime_clear_pooled_chunk_slot_internal(destination_slot);
            }
        }
    }

    for (int x = active_world_width; x < MAX_WORLD_WIDTH && spare_count > 0; ++x)
    for (int z = 0; z < MAX_WORLD_WIDTH && spare_count > 0; ++z)
    {
        if (world_get_chunk_slot_internal(x, z))
        {
            continue;
        }
        const Uint32 destination_slot = world_chunk_slot_id_for_index(x, z);
        world_runtime_clear_pooled_chunk_slot_internal(destination_slot);
        world_set_chunk_internal(x, z, spare_chunks[--spare_count]);
    }
    for (int x = 0; x < active_world_width && spare_count > 0; ++x)
    for (int z = active_world_width; z < MAX_WORLD_WIDTH && spare_count > 0; ++z)
    {
        if (world_get_chunk_slot_internal(x, z))
        {
            continue;
        }
        const Uint32 destination_slot = world_chunk_slot_id_for_index(x, z);
        world_runtime_clear_pooled_chunk_slot_internal(destination_slot);
        world_set_chunk_internal(x, z, spare_chunks[--spare_count]);
    }

    CHECK(!spare_count);
    world_runtime_commit_pooled_chunk_slot_reframe_internal();
    world_window_sort_chunks_internal();
    world_set_is_moving_internal(false);
}

void world_window_reset_at_origin_internal(int origin_x, int origin_z)
{
    world_set_origin_internal(origin_x, origin_z);
    for (int x = 0; x < MAX_WORLD_WIDTH; ++x)
    for (int z = 0; z < MAX_WORLD_WIDTH; ++z)
    {
        chunk_t* chunk = world_get_chunk_slot_internal(x, z);
        world_runtime_drop_pooled_chunk_slot_internal(chunk->slot_id);
        world_window_request_chunk(chunk, origin_x + x, origin_z + z);
    }
    world_runtime_flush_chunk_descriptors_internal();
    world_window_sort_chunks_internal();
    world_set_is_moving_internal(false);
}

void world_window_reset_internal(const camera_t* camera)
{
    const int origin_x = world_window_origin_for_position(camera->position[0]);
    const int origin_z = world_window_origin_for_position(camera->position[2]);
    world_window_reset_at_origin_internal(origin_x, origin_z);
}
