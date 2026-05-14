#include "world/jobs/scheduling.h"

#include "core/profile.h"
#include "world/jobs/taskflow/backend.h"
#include "world/runtime/internal.h"
#include "world/runtime/private.h"

namespace {

int g_regular_scan_cursor = 0;
int g_urgent_scan_cursor = 0;
int g_urgent_window_scan_cursor = 0;

typedef struct urgent_window
{
    int min_x;
    int max_x;
    int min_z;
    int max_z;
    bool valid;
}
urgent_window_t;

int env_int_clamped(const char* name, int fallback, int minimum, int maximum)
{
    const char* value = SDL_getenv(name);
    if (!value || value[0] == '\0')
    {
        return fallback;
    }
    return SDL_clamp(SDL_atoi(value), minimum, maximum);
}

int regular_scan_chunk_budget(int total)
{
    const int requested = env_int_clamped("OCTARYN_WORLD_REGULAR_SCAN_CHUNKS_PER_FRAME", 12, 1, SDL_max(1, total));
    return SDL_clamp(requested, 1, SDL_max(1, total));
}

int regular_submission_budget()
{
    return env_int_clamped("OCTARYN_WORLD_REGULAR_SUBMISSIONS_PER_FRAME", 1, 1, 8);
}

int urgent_submission_budget()
{
    return env_int_clamped("OCTARYN_WORLD_URGENT_SUBMISSIONS_PER_FRAME", 2, 1, 16);
}

int urgent_radius_chunks()
{
    return env_int_clamped("OCTARYN_WORLD_URGENT_RADIUS_CHUNKS", 5, 2, 12);
}

int urgent_scan_chunk_budget(int total)
{
    return env_int_clamped("OCTARYN_WORLD_URGENT_SCAN_CHUNKS_PER_FRAME", 8, 1, SDL_max(1, total));
}

Uint64 scan_time_budget_ns()
{
    static const int budget_us = env_int_clamped("OCTARYN_WORLD_SCAN_BUDGET_US", 500, 100, 2000);
    return static_cast<Uint64>(budget_us) * 1000ull;
}

bool scan_budget_expired(Uint64 start_ns, Uint64 budget_ns)
{
    return SDL_GetTicksNS() - start_ns >= budget_ns;
}

void record_probe_duration(float* max_ms, Uint64 start_ticks)
{
    if (!max_ms)
    {
        return;
    }
    *max_ms = SDL_max(*max_ms, oct_profile_elapsed_ms(start_ticks));
}

int local_chunk_for_world_position(float position, int origin)
{
    return static_cast<int>(SDL_floorf(position / static_cast<float>(CHUNK_WIDTH))) - origin;
}

urgent_window_t camera_urgent_window(const camera_t* camera)
{
    urgent_window_t window = {};
    if (!camera)
    {
        return window;
    }

    const int active_world_width = world_active_world_width_internal();
    const int center_x = local_chunk_for_world_position(camera->position[0], world_origin_x_internal());
    const int center_z = local_chunk_for_world_position(camera->position[2], world_origin_z_internal());
    const int radius = urgent_radius_chunks();
    window.min_x = SDL_max(1, center_x - radius);
    window.max_x = SDL_min(active_world_width - 2, center_x + radius);
    window.min_z = SDL_max(1, center_z - radius);
    window.max_z = SDL_min(active_world_width - 2, center_z + radius);
    window.valid = window.min_x <= window.max_x && window.min_z <= window.max_z;
    return window;
}

urgent_window_t expand_window(const urgent_window_t& window, int amount)
{
    if (!window.valid)
    {
        return window;
    }
    const int active_world_width = world_active_world_width_internal();
    urgent_window_t expanded = window;
    expanded.min_x = SDL_max(1, window.min_x - amount);
    expanded.max_x = SDL_min(active_world_width - 2, window.max_x + amount);
    expanded.min_z = SDL_max(1, window.min_z - amount);
    expanded.max_z = SDL_min(active_world_width - 2, window.max_z + amount);
    expanded.valid = expanded.min_x <= expanded.max_x && expanded.min_z <= expanded.max_z;
    return expanded;
}

void mark_urgent_window(const urgent_window_t& window)
{
    if (!window.valid)
    {
        return;
    }
    for (int x = window.min_x; x <= window.max_x; ++x)
    for (int z = window.min_z; z <= window.max_z; ++z)
    {
        world_mark_chunk_urgent(x, z);
    }
}

void scan_urgent_window(const urgent_window_t& window,
                        Uint64 scan_start_ns,
                        Uint64 scan_budget_ns,
                        world_jobs_schedule_report_t* report)
{
    if (!window.valid ||
        scan_budget_expired(scan_start_ns, scan_budget_ns) ||
        static_cast<int>(report->urgent_submissions) >= urgent_submission_budget())
    {
        return;
    }

    const int width = window.max_x - window.min_x + 1;
    const int depth = window.max_z - window.min_z + 1;
    const int total = width * depth;
    const int scan_count = SDL_min(total, urgent_scan_chunk_budget(total));
    if (g_urgent_window_scan_cursor < 0 || g_urgent_window_scan_cursor >= total)
    {
        g_urgent_window_scan_cursor = 0;
    }

    int scanned_chunks = 0;
    for (int scanned = 0; scanned < scan_count; ++scanned)
    {
        if (scan_budget_expired(scan_start_ns, scan_budget_ns))
        {
            break;
        }
        const int chunk_index = (g_urgent_window_scan_cursor + scanned) % total;
        const int x = window.min_x + chunk_index / depth;
        const int z = window.min_z + chunk_index % depth;
        ++scanned_chunks;
        const Uint64 probe_start = oct_profile_now_ticks();
        const bool submitted = world_jobs_taskflow_try_update_urgent_chunk(x, z);
        record_probe_duration(&report->urgent_probe_max_ms, probe_start);
        if (submitted)
        {
            report->urgent_submissions++;
            if (static_cast<int>(report->urgent_submissions) >= urgent_submission_budget())
            {
                break;
            }
        }
    }
    g_urgent_window_scan_cursor = (g_urgent_window_scan_cursor + scanned_chunks) % total;
}

void scan_budgeted_urgent_chunks(Uint64 scan_start_ns, Uint64 scan_budget_ns, world_jobs_schedule_report_t* report)
{
    if (scan_budget_expired(scan_start_ns, scan_budget_ns) ||
        static_cast<int>(report->urgent_submissions) >= urgent_submission_budget())
    {
        return;
    }

    const int total = world_active_world_width_internal() * world_active_world_width_internal();
    const int (*sorted_chunks)[2] = world_sorted_chunks_internal();
    const int scan_count = urgent_scan_chunk_budget(total);
    if (g_urgent_scan_cursor < 0 || g_urgent_scan_cursor >= total)
    {
        g_urgent_scan_cursor = 0;
    }
    int scanned_chunks = 0;
    for (int scanned = 0; scanned < scan_count; ++scanned)
    {
        if (scan_budget_expired(scan_start_ns, scan_budget_ns) ||
            static_cast<int>(report->urgent_submissions) >= urgent_submission_budget())
        {
            break;
        }
        const int chunk_index = (g_urgent_scan_cursor + scanned) % total;
        const int x = sorted_chunks[chunk_index][0];
        const int z = sorted_chunks[chunk_index][1];
        ++scanned_chunks;
        const Uint64 probe_start = oct_profile_now_ticks();
        const bool submitted = world_jobs_taskflow_try_update_urgent_chunk(x, z);
        record_probe_duration(&report->urgent_probe_max_ms, probe_start);
        if (submitted)
        {
            report->urgent_submissions++;
            if (static_cast<int>(report->urgent_submissions) >= urgent_submission_budget())
            {
                break;
            }
        }
    }
    g_urgent_scan_cursor = (g_urgent_scan_cursor + scanned_chunks) % total;
}

void scan_regular_chunks(Uint64 scan_start_ns, Uint64 scan_budget_ns, world_jobs_schedule_report_t* report)
{
    const int total = world_active_world_width_internal() * world_active_world_width_internal();
    const int (*sorted_chunks)[2] = world_sorted_chunks_internal();
    const Uint64 scan_start = oct_profile_now_ticks();
    const int scan_count = regular_scan_chunk_budget(total);
    const int submit_count = regular_submission_budget();
    int scanned_chunks = 0;
    if (g_regular_scan_cursor < 0 || g_regular_scan_cursor >= total)
    {
        g_regular_scan_cursor = 0;
    }

    for (int scanned = 0; scanned < scan_count; ++scanned)
    {
        if (scan_budget_expired(scan_start_ns, scan_budget_ns) || !world_jobs_taskflow_regular_slot_available())
        {
            break;
        }
        const int chunk_index = (g_regular_scan_cursor + scanned) % total;
        const int x = sorted_chunks[chunk_index][0];
        const int z = sorted_chunks[chunk_index][1];
        ++scanned_chunks;
        const Uint64 mesh_light_probe_start = oct_profile_now_ticks();
        const bool submitted_mesh_or_light = world_jobs_taskflow_try_update_meshes_or_lights(x, z);
        record_probe_duration(&report->regular_probe_max_ms, mesh_light_probe_start);
        if (submitted_mesh_or_light)
        {
            report->regular_submissions++;
            if (static_cast<int>(report->regular_submissions) >= submit_count)
            {
                break;
            }
        }
        else
        {
            const Uint64 block_probe_start = oct_profile_now_ticks();
            const bool submitted_blocks = world_jobs_taskflow_try_update_blocks(x, z);
            record_probe_duration(&report->regular_probe_max_ms, block_probe_start);
            if (submitted_blocks)
            {
                report->regular_submissions++;
                if (static_cast<int>(report->regular_submissions) >= submit_count)
                {
                    break;
                }
            }
        }
    }
    g_regular_scan_cursor = (g_regular_scan_cursor + scanned_chunks) % total;
    report->regular_scan_ms = oct_profile_elapsed_ms(scan_start);
}

} // namespace

void world_jobs_schedule_update(const camera_t* camera, bool allow_submissions, world_jobs_schedule_report_t* report)
{
    if (!report)
    {
        return;
    }

    *report = {};
    report->active_chunks = world_active_world_width_internal() * world_active_world_width_internal();
    if (!allow_submissions)
    {
        return;
    }

    const urgent_window_t urgent_window = camera_urgent_window(camera);
    mark_urgent_window(expand_window(urgent_window, 1));

    {
        OCT_PROFILE_ZONE("world_jobs.urgent_scan");
        const Uint64 scan_start = oct_profile_now_ticks();
        const Uint64 scan_start_ns = SDL_GetTicksNS();
        const Uint64 budget_ns = scan_time_budget_ns();
        scan_urgent_window(urgent_window, scan_start_ns, budget_ns, report);
        scan_budgeted_urgent_chunks(scan_start_ns, budget_ns, report);
        report->urgent_scan_ms = oct_profile_elapsed_ms(scan_start);
    }
    {
        OCT_PROFILE_ZONE("world_jobs.regular_scan");
        scan_regular_chunks(SDL_GetTicksNS(), scan_time_budget_ns(), report);
    }
}
