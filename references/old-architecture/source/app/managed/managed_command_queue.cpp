#include "app/managed/managed_command_queue.h"

#include "core/log.h"
#include "world/runtime/world.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

namespace {

constexpr std::size_t kInitialCommandCapacity = 256;
constexpr std::size_t kMaxCommandCapacity = 4096;
constexpr Uint64 kOverflowLogIntervalNs = SDL_NS_PER_SECOND;

struct managed_command_queue_t
{
    std::vector<oct_managed_native_command_t> commands;
    Uint64 last_overflow_log_ticks = 0;
    std::uint64_t dropped_non_critical = 0;
    std::uint64_t rejected_critical = 0;
};

struct managed_command_drain_counts_t
{
    std::uint32_t total = 0;
    std::uint32_t set_block = 0;
    std::uint32_t deferred = 0;
    std::uint32_t invalid = 0;
};

managed_command_queue_t g_command_queue;

bool is_critical_command(const oct_managed_native_command_t& command)
{
    if ((command.flags & OCT_MANAGED_NATIVE_COMMAND_FLAG_CRITICAL) != 0u)
    {
        return true;
    }

    switch (command.kind)
    {
    case OCT_MANAGED_NATIVE_COMMAND_SET_BLOCK:
    case OCT_MANAGED_NATIVE_COMMAND_SPAWN_PHYSICS_BODY:
    case OCT_MANAGED_NATIVE_COMMAND_DESTROY_PHYSICS_BODY:
    case OCT_MANAGED_NATIVE_COMMAND_NETWORK_EVENT:
        return true;
    default:
        return false;
    }
}

bool is_known_command_kind(std::uint32_t kind)
{
    switch (kind)
    {
    case OCT_MANAGED_NATIVE_COMMAND_SET_BLOCK:
    case OCT_MANAGED_NATIVE_COMMAND_SPAWN_PHYSICS_BODY:
    case OCT_MANAGED_NATIVE_COMMAND_DESTROY_PHYSICS_BODY:
    case OCT_MANAGED_NATIVE_COMMAND_SET_BODY_VELOCITY:
    case OCT_MANAGED_NATIVE_COMMAND_APPLY_BODY_IMPULSE:
    case OCT_MANAGED_NATIVE_COMMAND_RAYCAST:
    case OCT_MANAGED_NATIVE_COMMAND_NETWORK_EVENT:
        return true;
    default:
        return false;
    }
}

void log_overflow_if_due(const char* reason)
{
    const Uint64 now_ticks = SDL_GetTicksNS();
    if (g_command_queue.last_overflow_log_ticks != 0 &&
        now_ticks - g_command_queue.last_overflow_log_ticks < kOverflowLogIntervalNs)
    {
        return;
    }

    g_command_queue.last_overflow_log_ticks = now_ticks;
    oct_log_warnf("Managed command queue overflow: %s size=%zu capacity=%zu dropped_non_critical=%llu rejected_critical=%llu",
                  reason,
                  g_command_queue.commands.size(),
                  g_command_queue.commands.capacity(),
                  static_cast<unsigned long long>(g_command_queue.dropped_non_critical),
                  static_cast<unsigned long long>(g_command_queue.rejected_critical));
}

bool grow_if_possible()
{
    const std::size_t capacity = g_command_queue.commands.capacity();
    if (capacity >= kMaxCommandCapacity)
    {
        return false;
    }

    const std::size_t next_capacity = std::min(kMaxCommandCapacity, std::max(kInitialCommandCapacity, capacity * 2u));
    g_command_queue.commands.reserve(next_capacity);
    return true;
}

bool evict_non_critical_command()
{
    auto& commands = g_command_queue.commands;
    const auto it = std::find_if(commands.rbegin(), commands.rend(), [](const oct_managed_native_command_t& command) {
        return !is_critical_command(command);
    });
    if (it == commands.rend())
    {
        return false;
    }

    commands.erase(std::next(it).base());
    ++g_command_queue.dropped_non_critical;
    log_overflow_if_due("evicted non-critical command for critical command");
    return true;
}

void apply_set_block(const oct_managed_native_command_t& command, managed_command_drain_counts_t* counts)
{
    const int position[3] = {command.a, command.b, command.c};
    const int block_id = command.d;
    if (block_id < 0 || block_id >= BLOCK_COUNT || !world_queue_block_edit(position, static_cast<block_t>(block_id)))
    {
        ++counts->invalid;
        return;
    }

    ++counts->set_block;
}

void apply_command(const oct_managed_native_command_t& command, managed_command_drain_counts_t* counts)
{
    ++counts->total;
    switch (command.kind)
    {
    case OCT_MANAGED_NATIVE_COMMAND_SET_BLOCK:
        apply_set_block(command, counts);
        break;
    case OCT_MANAGED_NATIVE_COMMAND_SPAWN_PHYSICS_BODY:
    case OCT_MANAGED_NATIVE_COMMAND_DESTROY_PHYSICS_BODY:
    case OCT_MANAGED_NATIVE_COMMAND_SET_BODY_VELOCITY:
    case OCT_MANAGED_NATIVE_COMMAND_APPLY_BODY_IMPULSE:
    case OCT_MANAGED_NATIVE_COMMAND_RAYCAST:
    case OCT_MANAGED_NATIVE_COMMAND_NETWORK_EVENT:
        ++counts->deferred;
        break;
    default:
        ++counts->invalid;
        break;
    }
}

} // namespace

void app_managed_command_queue_init(void)
{
    g_command_queue = {};
    g_command_queue.commands.reserve(kInitialCommandCapacity);
}

void app_managed_command_queue_shutdown(void)
{
    g_command_queue = {};
}

int app_managed_command_queue_enqueue(const oct_managed_native_command_t* command)
{
    if (command == nullptr || !is_known_command_kind(command->kind))
    {
        return 0;
    }

    if (g_command_queue.commands.size() == g_command_queue.commands.capacity() && !grow_if_possible())
    {
        if (!is_critical_command(*command))
        {
            ++g_command_queue.dropped_non_critical;
            log_overflow_if_due("dropped incoming non-critical command");
            return 0;
        }

        if (!evict_non_critical_command())
        {
            ++g_command_queue.rejected_critical;
            log_overflow_if_due("critical queue is full");
            return 0;
        }
    }

    g_command_queue.commands.push_back(*command);
    return 1;
}

void app_managed_command_queue_drain(void)
{
    if (g_command_queue.commands.empty())
    {
        return;
    }

    managed_command_drain_counts_t counts = {};
    for (const oct_managed_native_command_t& command : g_command_queue.commands)
    {
        apply_command(command, &counts);
    }
    g_command_queue.commands.clear();

    if (counts.deferred != 0 || counts.invalid != 0)
    {
        oct_log_debugf("Managed command drain: total=%u set_block=%u deferred=%u invalid=%u",
                       counts.total,
                       counts.set_block,
                       counts.deferred,
                       counts.invalid);
    }
}
