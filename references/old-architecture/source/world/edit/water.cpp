#include "world/edit/environment.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <memory>
#include <vector>

#include "core/profile.h"
#include "world/direction.h"
#include "core/persistence/persistence.h"
#include "runtime/jobs/runtime_worker_pool.h"
#include "world/edit/light_dirty.h"
#include "world/edit/schedule.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"

namespace {

enum
{
    WATER_FLOW_DELAY_NS = 250000000,
    LAVA_FLOW_DELAY_NS = 500000000,
    WATER_MAX_DISPATCH_PER_SERVICE = 256,
    WATER_MAX_APPLY_PER_SERVICE = 128,
    WATER_REPAIR_SCAN_BLOCKS_PER_SERVICE = 4096,
    WATER_REPAIR_MAX_SCHEDULE_PER_SERVICE = 64,
};

struct water_update_t
{
    int position[3] = {};
    Uint64 due_ticks = 0;
};

struct water_sample_t
{
    int position[3] = {};
    block_t current = BLOCK_EMPTY;
    block_t above = BLOCK_EMPTY;
    block_t below = BLOCK_EMPTY;
    block_t neighbors[4] = {};
    bool neighbor_can_spread_here[4] = {};
    bool below_supports_source = false;
};

struct water_result_t
{
    int position[3] = {};
    block_t expected = BLOCK_EMPTY;
    block_t next = BLOCK_EMPTY;
};

struct water_batch_t
{
    std::atomic_bool ready{false};
    size_t next_result = 0;
    std::vector<water_sample_t> samples;
    std::vector<water_result_t> results;
};

enum water_sample_status_t
{
    WATER_SAMPLE_READY,
    WATER_SAMPLE_UNAVAILABLE,
    WATER_SAMPLE_BLOCKED,
};

std::vector<water_update_t> g_water_updates;
std::shared_ptr<water_batch_t> g_active_water_batch;
int g_water_repair_chunk_x = 1;
int g_water_repair_chunk_z = 1;
int g_water_repair_block_index = 0;

water_sample_status_t make_water_sample(const water_update_t& update, water_sample_t* sample);
void schedule_water_neighborhood(const int position[3], Uint64 due_ticks);

bool is_replaceable_by_fluid(block_t block)
{
    return block == BLOCK_LEAVES || block_requires_grass(block);
}

Uint64 fluid_flow_delay_ns(fluid_kind_t kind)
{
    return kind == FLUID_LAVA ? LAVA_FLOW_DELAY_NS : WATER_FLOW_DELAY_NS;
}

bool try_get_loaded_block(const int position[3], block_t* block)
{
    int chunk_x = 0;
    int chunk_z = 0;
    chunk_t* chunk = NULL;
    if (!world_try_get_loaded_chunk_at(position, &chunk_x, &chunk_z, &chunk))
    {
        return false;
    }
    *block = world_chunk_get_block(chunk, position[0], position[1], position[2]);
    return true;
}

void mark_chunk_if_edit_visible(bool touched[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH], int chunk_x, int chunk_z)
{
    if (world_chunk_is_local_index(chunk_x, chunk_z) && !world_chunk_is_border_index(chunk_x, chunk_z))
    {
        touched[chunk_x][chunk_z] = true;
    }
}

void mark_water_chunks_for_regen(bool touched[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH],
                                 int chunk_x,
                                 int chunk_z,
                                 int local_x,
                                 int local_z)
{
    mark_chunk_if_edit_visible(touched, chunk_x, chunk_z);
    if (local_x == 0)
    {
        mark_chunk_if_edit_visible(touched, chunk_x - 1, chunk_z);
    }
    else if (local_x == CHUNK_WIDTH - 1)
    {
        mark_chunk_if_edit_visible(touched, chunk_x + 1, chunk_z);
    }

    if (local_z == 0)
    {
        mark_chunk_if_edit_visible(touched, chunk_x, chunk_z - 1);
    }
    else if (local_z == CHUNK_WIDTH - 1)
    {
        mark_chunk_if_edit_visible(touched, chunk_x, chunk_z + 1);
    }

    if (local_x == 0 && local_z == 0)
    {
        mark_chunk_if_edit_visible(touched, chunk_x - 1, chunk_z - 1);
    }
    else if (local_x == 0 && local_z == CHUNK_WIDTH - 1)
    {
        mark_chunk_if_edit_visible(touched, chunk_x - 1, chunk_z + 1);
    }
    else if (local_x == CHUNK_WIDTH - 1 && local_z == 0)
    {
        mark_chunk_if_edit_visible(touched, chunk_x + 1, chunk_z - 1);
    }
    else if (local_x == CHUNK_WIDTH - 1 && local_z == CHUNK_WIDTH - 1)
    {
        mark_chunk_if_edit_visible(touched, chunk_x + 1, chunk_z + 1);
    }
}

bool set_world_block_raw(
    const int position[3],
    block_t block,
    bool persist,
    bool touched_mesh[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH],
    bool touched_skylight[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH])
{
    int chunk_x = 0;
    int chunk_z = 0;
    chunk_t* chunk = NULL;
    if (!world_try_get_loaded_chunk_at(position, &chunk_x, &chunk_z, &chunk))
    {
        return false;
    }
    block_t old_block = world_chunk_get_block(chunk, position[0], position[1], position[2]);
    if (old_block == block)
    {
        return false;
    }
    const Uint32 light_flags = world_edit_get_light_dirty_flags(old_block, block);
    if (persist)
    {
        persistence_set_block(world_chunk_world_x(chunk), world_chunk_world_z(chunk), position[0], position[1], position[2], block);
    }
    int local_x = position[0];
    int local_y = position[1];
    int local_z = position[2];
    world_chunk_to_local(chunk, &local_x, &local_y, &local_z);
    world_chunk_set_block(chunk, position[0], position[1], position[2], block);
    mark_water_chunks_for_regen(touched_mesh, chunk_x, chunk_z, local_x, local_z);
    if ((light_flags & WORLD_LIGHT_DIRTY_SKYLIGHT) != 0)
    {
        world_edit_accumulate_skylight_regen_chunks(touched_skylight, chunk_x, chunk_z, local_x, local_z);
    }
    return true;
}

bool block_can_accept_fluid(block_t block)
{
    return block == BLOCK_EMPTY || is_replaceable_by_fluid(block) || block_is_fluid(block);
}

bool block_can_accept_specific_fluid(block_t block, fluid_kind_t kind)
{
    if (block == BLOCK_EMPTY || is_replaceable_by_fluid(block))
    {
        return true;
    }
    return block_get_fluid_kind(block) == kind;
}

bool fluid_can_flow_down_from(const int position[3], fluid_kind_t kind)
{
    if (kind == FLUID_NONE || position[1] <= 0)
    {
        return false;
    }
    const int below[3] = {position[0], position[1] - 1, position[2]};
    block_t below_block = BLOCK_EMPTY;
    if (!try_get_loaded_block(below, &below_block))
    {
        return false;
    }
    return !(block_get_fluid_kind(below_block) == kind && block_is_fluid_source(below_block)) &&
        block_can_accept_specific_fluid(below_block, kind);
}

bool horizontal_position_can_accept_fluid(const int position[3], fluid_kind_t kind)
{
    block_t block = BLOCK_EMPTY;
    if (!try_get_loaded_block(position, &block))
    {
        return false;
    }
    if (block_get_fluid_kind(block) == kind && block_is_fluid_source(block))
    {
        return false;
    }
    return block_can_accept_specific_fluid(block, kind);
}

int source_neighbor_count_for_fluid(const int position[3], fluid_kind_t kind)
{
    int count = 0;
    for (int i = 0; i < 4; ++i)
    {
        const int neighbor[3] = {
            position[0] + DIRECTIONS[i][0],
            position[1],
            position[2] + DIRECTIONS[i][2],
        };
        block_t block = BLOCK_EMPTY;
        if (try_get_loaded_block(neighbor, &block) && block_get_fluid_kind(block) == kind && block_is_fluid_source(block))
        {
            ++count;
        }
    }
    return count;
}

int fluid_slope_find_distance(fluid_kind_t kind)
{
    return kind == FLUID_LAVA ? 2 : 4;
}

int fluid_slope_distance_to_drop(const int position[3], fluid_kind_t kind, int pass, int from_direction)
{
    int lowest = 1000;
    const int max_pass = fluid_slope_find_distance(kind);
    for (int direction = 0; direction < 4; ++direction)
    {
        if (direction == from_direction)
        {
            continue;
        }
        const int target[3] = {
            position[0] + DIRECTIONS[direction][0],
            position[1],
            position[2] + DIRECTIONS[direction][2],
        };
        if (!horizontal_position_can_accept_fluid(target, kind))
        {
            continue;
        }
        if (fluid_can_flow_down_from(target, kind))
        {
            return pass;
        }
        if (pass < max_pass)
        {
            const int reverse_direction = direction ^ 1;
            const int distance = fluid_slope_distance_to_drop(target, kind, pass + 1, reverse_direction);
            lowest = SDL_min(lowest, distance);
        }
    }
    return lowest;
}

bool fluid_neighbor_would_spread_to(const int donor[3], int target_direction, fluid_kind_t kind)
{
    block_t donor_block = BLOCK_EMPTY;
    if (!try_get_loaded_block(donor, &donor_block) || block_get_fluid_kind(donor_block) != kind)
    {
        return false;
    }
    const int donor_level = block_get_fluid_level(donor_block);
    if (donor_level < 0 || donor_level >= 7)
    {
        return false;
    }
    const int target[3] = {
        donor[0] + DIRECTIONS[target_direction][0],
        donor[1],
        donor[2] + DIRECTIONS[target_direction][2],
    };
    if (!horizontal_position_can_accept_fluid(target, kind))
    {
        return false;
    }

    if (fluid_can_flow_down_from(donor, kind) && source_neighbor_count_for_fluid(donor, kind) < 3)
    {
        return false;
    }

    int best_distance = 1000;
    int target_distance = 1000;
    for (int direction = 0; direction < 4; ++direction)
    {
        const int candidate[3] = {
            donor[0] + DIRECTIONS[direction][0],
            donor[1],
            donor[2] + DIRECTIONS[direction][2],
        };
        if (!horizontal_position_can_accept_fluid(candidate, kind))
        {
            continue;
        }

        int distance = 1000;
        if (fluid_can_flow_down_from(candidate, kind))
        {
            distance = 0;
        }
        else
        {
            distance = fluid_slope_distance_to_drop(candidate, kind, 1, direction ^ 1);
        }

        best_distance = SDL_min(best_distance, distance);
        if (direction == target_direction)
        {
            target_distance = distance;
        }
    }

    return target_distance == best_distance;
}

bool fluid_can_spread_from_position(const int position[3], fluid_kind_t kind)
{
    if (fluid_can_flow_down_from(position, kind))
    {
        return true;
    }
    for (int direction = 0; direction < 4; ++direction)
    {
        if (fluid_neighbor_would_spread_to(position, direction, kind))
        {
            return true;
        }
    }
    return false;
}

bool horizontal_fluid_donor_is_eligible(const water_sample_t& sample, int index, fluid_kind_t kind)
{
    const block_t neighbor = sample.neighbors[index];
    return block_get_fluid_kind(neighbor) == kind &&
        block_get_fluid_level(neighbor) >= 0 &&
        block_get_fluid_level(neighbor) < 7 &&
        sample.neighbor_can_spread_here[index];
}

bool fluid_can_enter_from_side(const water_sample_t& sample, fluid_kind_t kind)
{
    for (int i = 0; i < 4; ++i)
    {
        if (horizontal_fluid_donor_is_eligible(sample, i, kind))
        {
            return true;
        }
    }
    return false;
}

bool sample_has_fluid_kind(const water_sample_t& sample, fluid_kind_t kind)
{
    if (block_get_fluid_kind(sample.above) == kind)
    {
        return true;
    }
    for (int i = 0; i < 4; ++i)
    {
        if (block_get_fluid_kind(sample.neighbors[i]) == kind && sample.neighbor_can_spread_here[i])
        {
            return true;
        }
    }
    return false;
}

bool sample_touches_fluid_kind(const water_sample_t& sample, fluid_kind_t kind)
{
    if (block_get_fluid_kind(sample.above) == kind || block_get_fluid_kind(sample.below) == kind)
    {
        return true;
    }
    for (block_t neighbor : sample.neighbors)
    {
        if (block_get_fluid_kind(neighbor) == kind)
        {
            return true;
        }
    }
    return false;
}

bool sample_has_opposing_fluid(const water_sample_t& sample, fluid_kind_t kind)
{
    return kind != FLUID_NONE &&
        sample_touches_fluid_kind(sample, kind == FLUID_WATER ? FLUID_LAVA : FLUID_WATER);
}

fluid_kind_t sample_incoming_fluid_kind(const water_sample_t& sample)
{
    const bool has_water = sample_has_fluid_kind(sample, FLUID_WATER);
    const bool has_lava = sample_has_fluid_kind(sample, FLUID_LAVA);
    if (has_water == has_lava)
    {
        return FLUID_NONE;
    }
    return has_water ? FLUID_WATER : FLUID_LAVA;
}

block_t fluid_from_direct_contact(const water_sample_t& sample, fluid_kind_t kind)
{
    if (block_get_fluid_kind(sample.above) == kind)
    {
        return block_make_fluid(kind, 1);
    }

    int next_level = 99;
    for (int i = 0; i < 4; ++i)
    {
        if (!horizontal_fluid_donor_is_eligible(sample, i, kind))
        {
            continue;
        }

        const int level = block_get_fluid_level(sample.neighbors[i]);
        if (level >= 0)
        {
            next_level = SDL_min(next_level, SDL_min(level + 1, 7));
        }
    }
    return next_level <= 7 ? block_make_fluid(kind, next_level) : sample.current;
}

bool position_can_support_water_source(const int position[3])
{
    block_t block = BLOCK_EMPTY;
    return try_get_loaded_block(position, &block) && (block_is_solid(block) || block_is_water_source(block));
}

block_t compute_water_sample(const water_sample_t& sample)
{
    const fluid_kind_t current_kind = block_get_fluid_kind(sample.current);
    const fluid_kind_t incoming_kind = sample_incoming_fluid_kind(sample);

    if (current_kind == FLUID_LAVA && sample_has_opposing_fluid(sample, current_kind))
    {
        return BLOCK_STONE;
    }

    const fluid_kind_t kind = current_kind != FLUID_NONE ? current_kind : incoming_kind;
    if (kind == FLUID_NONE)
    {
        return sample.current;
    }

    if (block_is_fluid_source(sample.current))
    {
        return sample.current;
    }

    if (is_replaceable_by_fluid(sample.current) && block_get_fluid_kind(sample.above) != kind && !fluid_can_enter_from_side(sample, kind))
    {
        return sample.current;
    }
    if (is_replaceable_by_fluid(sample.current))
    {
        return fluid_from_direct_contact(sample, kind);
    }

    if (kind == FLUID_WATER)
    {
        int source_count = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (block_is_water_source(sample.neighbors[i]) && sample.neighbor_can_spread_here[i])
            {
                ++source_count;
            }
        }
        if (source_count >= 2 && sample.below_supports_source)
        {
            return BLOCK_WATER;
        }
    }

    if (block_get_fluid_kind(sample.above) == kind)
    {
        return block_make_fluid(kind, 1);
    }

    int next_level = 99;
    for (int i = 0; i < 4; ++i)
    {
        const block_t neighbor = sample.neighbors[i];
        if (!horizontal_fluid_donor_is_eligible(sample, i, kind))
        {
            continue;
        }

        const int level = block_get_fluid_level(neighbor);
        if (level >= 0 && level < 7)
        {
            next_level = SDL_min(next_level, level + 1);
        }
    }

    return next_level <= 7 ? block_make_fluid(kind, next_level) : BLOCK_EMPTY;
}

bool liquid_position_needs_repair(const int position[3], block_t current)
{
    const fluid_kind_t kind = block_get_fluid_kind(current);
    if (kind == FLUID_NONE)
    {
        return false;
    }
    water_sample_t sample = {};
    water_update_t update = {};
    update.position[0] = position[0];
    update.position[1] = position[1];
    update.position[2] = position[2];
    if (make_water_sample(update, &sample) != WATER_SAMPLE_READY)
    {
        return false;
    }
    if (kind == FLUID_LAVA && sample_has_opposing_fluid(sample, kind))
    {
        return true;
    }
    if (fluid_can_spread_from_position(position, kind))
    {
        return true;
    }
    return !block_is_fluid_source(current) && compute_water_sample(sample) != current;
}

void advance_liquid_repair_cursor(int active_world_width)
{
    ++g_water_repair_block_index;
    if (g_water_repair_block_index < CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH)
    {
        return;
    }

    g_water_repair_block_index = 0;
    ++g_water_repair_chunk_z;
    if (g_water_repair_chunk_z < active_world_width - 1)
    {
        return;
    }

    g_water_repair_chunk_z = 1;
    ++g_water_repair_chunk_x;
    if (g_water_repair_chunk_x >= active_world_width - 1)
    {
        g_water_repair_chunk_x = 1;
    }
}

void service_stale_liquid_repair(Uint64 now_ticks)
{
    const int active_world_width = world_active_world_width_internal();
    if (active_world_width <= 2)
    {
        return;
    }
    g_water_repair_chunk_x = SDL_clamp(g_water_repair_chunk_x, 1, active_world_width - 2);
    g_water_repair_chunk_z = SDL_clamp(g_water_repair_chunk_z, 1, active_world_width - 2);
    g_water_repair_block_index = SDL_clamp(g_water_repair_block_index, 0, CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_WIDTH - 1);

    int scanned = 0;
    int scheduled = 0;
    while (scanned < WATER_REPAIR_SCAN_BLOCKS_PER_SERVICE && scheduled < WATER_REPAIR_MAX_SCHEDULE_PER_SERVICE)
    {
        chunk_t* chunk = world_get_chunk_internal(g_water_repair_chunk_x, g_water_repair_chunk_z);
        const int block_index = g_water_repair_block_index;
        advance_liquid_repair_cursor(active_world_width);
        ++scanned;

        if (!chunk || SDL_GetAtomicInt(&chunk->block_state) != JOB_STATE_COMPLETED)
        {
            continue;
        }

        const int bx = block_index / (CHUNK_HEIGHT * CHUNK_WIDTH);
        const int remainder = block_index - bx * CHUNK_HEIGHT * CHUNK_WIDTH;
        const int by = remainder / CHUNK_WIDTH;
        const int bz = remainder - by * CHUNK_WIDTH;

        SDL_LockMutex(chunk->data_mutex);
        const block_t block = world_chunk_read_local_block(chunk, bx, by, bz);
        SDL_UnlockMutex(chunk->data_mutex);
        if (!block_is_fluid(block))
        {
            continue;
        }

        const int position[3] = {chunk->position[0] + bx, by, chunk->position[1] + bz};
        if (!liquid_position_needs_repair(position, block))
        {
            continue;
        }

        schedule_water_neighborhood(position, now_ticks);
        ++scheduled;
    }
}

water_sample_status_t make_water_sample(const water_update_t& update, water_sample_t* sample)
{
    if (!sample)
    {
        return WATER_SAMPLE_BLOCKED;
    }

    *sample = {};
    sample->position[0] = update.position[0];
    sample->position[1] = update.position[1];
    sample->position[2] = update.position[2];
    if (!try_get_loaded_block(sample->position, &sample->current))
    {
        return WATER_SAMPLE_UNAVAILABLE;
    }
    if (!block_can_accept_fluid(sample->current))
    {
        return WATER_SAMPLE_BLOCKED;
    }

    if (sample->position[1] + 1 < CHUNK_HEIGHT)
    {
        const int above[3] = {sample->position[0], sample->position[1] + 1, sample->position[2]};
        try_get_loaded_block(above, &sample->above);
    }

    if (sample->position[1] > 0)
    {
        const int below[3] = {sample->position[0], sample->position[1] - 1, sample->position[2]};
        try_get_loaded_block(below, &sample->below);
        sample->below_supports_source = position_can_support_water_source(below);
    }

    for (int i = 0; i < 4; ++i)
    {
        const int neighbor[3] = {
            sample->position[0] + DIRECTIONS[i][0],
            sample->position[1],
            sample->position[2] + DIRECTIONS[i][2],
        };
        try_get_loaded_block(neighbor, &sample->neighbors[i]);
        sample->neighbor_can_spread_here[i] = fluid_neighbor_would_spread_to(neighbor, i ^ 1,
                                                                             block_get_fluid_kind(sample->neighbors[i]));
    }
    return WATER_SAMPLE_READY;
}

void schedule_water_position(const int position[3], Uint64 due_ticks)
{
    if (!position || position[1] < 0 || position[1] >= CHUNK_HEIGHT)
    {
        return;
    }
    for (water_update_t& update : g_water_updates)
    {
        if (update.position[0] == position[0] &&
            update.position[1] == position[1] &&
            update.position[2] == position[2])
        {
            update.due_ticks = SDL_min(update.due_ticks, due_ticks);
            return;
        }
    }

    water_update_t update = {};
    update.position[0] = position[0];
    update.position[1] = position[1];
    update.position[2] = position[2];
    update.due_ticks = due_ticks;
    g_water_updates.push_back(update);
}

void schedule_water_neighborhood(const int position[3], Uint64 due_ticks)
{
    schedule_water_position(position, due_ticks);
    if (position[1] + 1 < CHUNK_HEIGHT)
    {
        const int above[3] = {position[0], position[1] + 1, position[2]};
        schedule_water_position(above, due_ticks);
    }
    if (position[1] > 0)
    {
        const int below[3] = {position[0], position[1] - 1, position[2]};
        schedule_water_position(below, due_ticks);
    }
    for (int i = 0; i < 4; ++i)
    {
        const int neighbor[3] = {
            position[0] + DIRECTIONS[i][0],
            position[1],
            position[2] + DIRECTIONS[i][2],
        };
        schedule_water_position(neighbor, due_ticks);
    }
}

bool apply_water_result(const water_result_t& result,
                        bool touched_mesh[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH],
                        bool touched_skylight[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH])
{
    const int position[3] = {result.position[0], result.position[1], result.position[2]};
    block_t current = BLOCK_EMPTY;
    if (!try_get_loaded_block(position, &current))
    {
        schedule_water_position(position, SDL_GetTicksNS() + WATER_FLOW_DELAY_NS);
        return false;
    }
    if (current != result.expected)
    {
        if (block_can_accept_fluid(current))
        {
            fluid_kind_t kind = block_get_fluid_kind(current);
            if (kind == FLUID_NONE)
            {
                kind = block_get_fluid_kind(result.next);
            }
            schedule_water_neighborhood(position, SDL_GetTicksNS() + fluid_flow_delay_ns(kind));
        }
        return false;
    }
    if (!block_is_fluid(current) && !block_is_fluid(result.next) && result.next != BLOCK_STONE)
    {
        return false;
    }

    const bool changed = set_world_block_raw(position, result.next, true, touched_mesh, touched_skylight);
    if (changed)
    {
        fluid_kind_t kind = block_get_fluid_kind(result.next);
        if (kind == FLUID_NONE)
        {
            kind = block_get_fluid_kind(current);
        }
        const Uint64 now_ticks = SDL_GetTicksNS();
        const Uint64 due_ticks = result.next == BLOCK_STONE ? now_ticks : now_ticks + fluid_flow_delay_ns(kind);
        schedule_water_neighborhood(position, due_ticks);
    }
    return changed;
}

void finalize_water_service(bool touched_mesh[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH],
                            bool touched_skylight[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH])
{
    for (int x = 0; x < MAX_WORLD_WIDTH; ++x)
    for (int z = 0; z < MAX_WORLD_WIDTH; ++z)
    {
        if (touched_mesh[x][z])
        {
            world_edit_request_urgent_mesh_regen_for_chunk(x, z);
        }
    }
    world_edit_request_skylight_regen_for_chunks(touched_skylight);
}

void compute_water_batch(water_batch_t* batch)
{
    if (!batch)
    {
        return;
    }

    batch->results.clear();
    batch->results.reserve(batch->samples.size());
    for (const water_sample_t& sample : batch->samples)
    {
        const block_t next = compute_water_sample(sample);
        if (!block_is_fluid(sample.current) && !block_is_fluid(next) && next != BLOCK_STONE)
        {
            continue;
        }

        water_result_t result = {};
        result.position[0] = sample.position[0];
        result.position[1] = sample.position[1];
        result.position[2] = sample.position[2];
        result.expected = sample.current;
        result.next = next;
        batch->results.push_back(result);
    }
    batch->ready.store(true, std::memory_order_release);
}

bool apply_ready_water_batch(Uint64 start_ticks,
                             Uint64 budget_ns,
                             bool touched_mesh[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH],
                             bool touched_skylight[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH],
                             int* applied_steps)
{
    if (!g_active_water_batch || !g_active_water_batch->ready.load(std::memory_order_acquire))
    {
        return false;
    }

    int applied = 0;
    while (g_active_water_batch->next_result < g_active_water_batch->results.size())
    {
        const water_result_t& result = g_active_water_batch->results[g_active_water_batch->next_result];
        apply_water_result(result, touched_mesh, touched_skylight);
        ++g_active_water_batch->next_result;
        ++applied;
        if (applied >= WATER_MAX_APPLY_PER_SERVICE || SDL_GetTicksNS() - start_ticks >= budget_ns)
        {
            break;
        }
    }

    if (g_active_water_batch->next_result < g_active_water_batch->results.size())
    {
        if (applied_steps)
        {
            *applied_steps += applied;
        }
        return true;
    }

    if (applied_steps)
    {
        *applied_steps += applied;
    }
    g_active_water_batch.reset();
    return true;
}

bool dispatch_water_batch(Uint64 now_ticks)
{
    if (g_active_water_batch)
    {
        return false;
    }

    auto batch = std::make_shared<water_batch_t>();
    batch->samples.reserve(WATER_MAX_DISPATCH_PER_SERVICE);
    for (size_t i = 0; i < g_water_updates.size() &&
                       batch->samples.size() < static_cast<size_t>(WATER_MAX_DISPATCH_PER_SERVICE);)
    {
        if (g_water_updates[i].due_ticks > now_ticks)
        {
            ++i;
            continue;
        }

        water_sample_t sample = {};
        const water_sample_status_t sample_status = make_water_sample(g_water_updates[i], &sample);
        if (sample_status == WATER_SAMPLE_UNAVAILABLE)
        {
            g_water_updates[i].due_ticks = now_ticks + WATER_FLOW_DELAY_NS;
            ++i;
            continue;
        }

        g_water_updates[i] = g_water_updates.back();
        g_water_updates.pop_back();
        if (sample_status == WATER_SAMPLE_READY)
        {
            batch->samples.push_back(sample);
        }
    }

    if (batch->samples.empty())
    {
        return false;
    }

    runtime_worker_pool_t& worker_pool = runtime_worker_pool_shared();
    if (worker_pool.submit([batch]() {
            compute_water_batch(batch.get());
        }))
    {
        g_active_water_batch = batch;
        return true;
    }

    compute_water_batch(batch.get());
    g_active_water_batch = batch;
    return true;
}

} // namespace

bool world_edit_should_update_water_region(const int center[3], block_t old_block, block_t new_block)
{
    if (block_is_fluid(old_block) || block_is_fluid(new_block))
    {
        return true;
    }

    block_t block = BLOCK_EMPTY;
    int above[3] = {center[0], center[1] + 1, center[2]};
    if (center[1] + 1 < CHUNK_HEIGHT && try_get_loaded_block(above, &block) && block_is_fluid(block))
    {
        return true;
    }
    int below[3] = {center[0], center[1] - 1, center[2]};
    if (center[1] > 0 && try_get_loaded_block(below, &block) && block_is_fluid(block))
    {
        return true;
    }
    for (int i = 0; i < 4; ++i)
    {
        int neighbor[3] = {
            center[0] + DIRECTIONS[i][0],
            center[1],
            center[2] + DIRECTIONS[i][2],
        };
        if (try_get_loaded_block(neighbor, &block) && block_is_fluid(block))
        {
            return true;
        }
    }
    return false;
}

void world_edit_update_water_region(const int center[3])
{
    OCT_PROFILE_ZONE("world_edit.water_region");
    if (!center)
    {
        return;
    }
    block_t block = BLOCK_EMPTY;
    fluid_kind_t kind = FLUID_WATER;
    if (try_get_loaded_block(center, &block) && block_get_fluid_kind(block) != FLUID_NONE)
    {
        kind = block_get_fluid_kind(block);
    }
    schedule_water_neighborhood(center, SDL_GetTicksNS() + fluid_flow_delay_ns(kind));
}

void world_edit_service_water_updates(Uint64 budget_ns)
{
    if (budget_ns == 0)
    {
        return;
    }

    OCT_PROFILE_ZONE("world_edit.service_water_updates");
    const Uint64 start_ticks = SDL_GetTicksNS();
    const Uint64 now_ticks = start_ticks;
    service_stale_liquid_repair(now_ticks);
    if (g_water_updates.empty() && !g_active_water_batch)
    {
        return;
    }

    bool touched_mesh[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH] = {};
    bool touched_skylight[MAX_WORLD_WIDTH][MAX_WORLD_WIDTH] = {};
    int steps = 0;
    apply_ready_water_batch(start_ticks, budget_ns, touched_mesh, touched_skylight, &steps);
    if (SDL_GetTicksNS() - start_ticks < budget_ns)
    {
        dispatch_water_batch(now_ticks);
    }
    if (steps > 0)
    {
        finalize_water_service(touched_mesh, touched_skylight);
    }
}
