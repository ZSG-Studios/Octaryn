#include <SDL3/SDL.h>

#include <vector>

#include "world/edit/internal.h"
#include "world/edit/environment.h"
#include "world/runtime/internal.h"

namespace {

constexpr Uint64 kEditApplyBudgetNs = 750000ull;
constexpr Uint64 kWaterApplyBudgetNs = 1500000ull;

struct pending_block_edit_t
{
    int position[3];
    block_t block;
    Uint64 queued_at_ticks;
};

std::vector<pending_block_edit_t> g_pending_block_edits;
Uint64 g_queued_edits = 0;
Uint64 g_applied_edits = 0;
Uint64 g_changed_edits = 0;
Uint64 g_deferred_apply_attempts = 0;
Uint64 g_last_changed_edit_ticks = 0;

} // namespace

bool world_queue_block_edit(const int position[3], block_t block)
{
    if (!position || position[1] < 0 || position[1] >= CHUNK_HEIGHT)
    {
        return false;
    }

    pending_block_edit_t edit = {};
    edit.position[0] = position[0];
    edit.position[1] = position[1];
    edit.position[2] = position[2];
    edit.block = block;
    edit.queued_at_ticks = SDL_GetTicksNS();
    g_pending_block_edits.push_back(edit);
    ++g_queued_edits;
    return true;
}

void world_apply_pending_block_edits(void)
{
    const Uint64 start_ticks = SDL_GetTicksNS();
    for (size_t i = 0; i < g_pending_block_edits.size();)
    {
        const pending_block_edit_t edit = g_pending_block_edits[i];
        const world_block_edit_apply_result_t result = world_try_apply_block_edit_detailed(edit.position, edit.block);
        if (result.applied)
        {
            ++g_applied_edits;
            if (result.changed)
            {
                ++g_changed_edits;
                g_last_changed_edit_ticks = SDL_GetTicksNS();
            }
            const float latency_ms = static_cast<float>(SDL_GetTicksNS() - edit.queued_at_ticks) * 1e-6f;
            if (latency_ms >= 50.0f)
            {
                SDL_Log("Edit timing | queued block edit at (%d, %d, %d) applied after %.2f ms",
                        edit.position[0], edit.position[1], edit.position[2], latency_ms);
            }
            g_pending_block_edits[i] = g_pending_block_edits.back();
            g_pending_block_edits.pop_back();
            if (SDL_GetTicksNS() - start_ticks >= kEditApplyBudgetNs)
            {
                break;
            }
            continue;
        }
        ++g_deferred_apply_attempts;
        ++i;
    }
    const Uint64 elapsed = SDL_GetTicksNS() - start_ticks;
    const Uint64 leftover_budget = elapsed < kEditApplyBudgetNs ? kEditApplyBudgetNs - elapsed : 0;
    world_edit_service_water_updates(kWaterApplyBudgetNs + leftover_budget);
}

void world_get_edit_debug_stats(world_edit_debug_stats_t* out_stats)
{
    if (!out_stats)
    {
        return;
    }
    *out_stats = {};
    out_stats->queued = g_queued_edits;
    out_stats->applied = g_applied_edits;
    out_stats->changed = g_changed_edits;
    out_stats->deferred_attempts = g_deferred_apply_attempts;
    out_stats->pending = static_cast<Uint32>(g_pending_block_edits.size());
}

bool world_edit_recent_changes(Uint64 window_ns)
{
    if (g_last_changed_edit_ticks == 0u)
    {
        return false;
    }
    return SDL_GetTicksNS() - g_last_changed_edit_ticks <= window_ns;
}
