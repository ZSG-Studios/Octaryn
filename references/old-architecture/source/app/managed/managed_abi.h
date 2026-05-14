#pragma once

#include <cstddef>
#include <cstdint>

enum oct_managed_native_command_kind : std::uint32_t
{
    OCT_MANAGED_NATIVE_COMMAND_NONE = 0,
    OCT_MANAGED_NATIVE_COMMAND_SET_BLOCK = 1,
    OCT_MANAGED_NATIVE_COMMAND_SPAWN_PHYSICS_BODY = 2,
    OCT_MANAGED_NATIVE_COMMAND_DESTROY_PHYSICS_BODY = 3,
    OCT_MANAGED_NATIVE_COMMAND_SET_BODY_VELOCITY = 4,
    OCT_MANAGED_NATIVE_COMMAND_APPLY_BODY_IMPULSE = 5,
    OCT_MANAGED_NATIVE_COMMAND_RAYCAST = 6,
    OCT_MANAGED_NATIVE_COMMAND_NETWORK_EVENT = 7,
};

struct oct_managed_native_command_t
{
    std::uint32_t kind;
    std::uint32_t flags;
    std::uint64_t request_id;
    std::uint64_t target_id;
    std::int32_t a;
    std::int32_t b;
    std::int32_t c;
    std::int32_t d;
    float x;
    float y;
    float z;
    float w;
    float x2;
    float y2;
    float z2;
    float w2;
    std::uint64_t payload0;
    std::uint64_t payload1;
};

struct oct_managed_input_snapshot_t
{
    std::uint32_t version;
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t controller;
    float move_x;
    float move_y;
    float move_z;
    float camera_x;
    float camera_y;
    float camera_z;
    float camera_pitch;
    float camera_yaw;
    std::int32_t relative_mouse;
    std::int32_t reserved;
};

struct oct_managed_world_snapshot_t
{
    std::uint32_t version;
    std::uint32_t size;
    std::uint64_t frame_index;
    std::uint64_t day_index;
    double delta_seconds;
    double total_world_seconds;
    std::uint32_t second_of_day;
    std::int32_t player_block_x;
    std::int32_t player_block_y;
    std::int32_t player_block_z;
    std::int32_t render_distance;
    std::int32_t active_chunks;
    std::int32_t loaded_chunks;
    std::int32_t mesh_ready_chunks;
    std::int32_t running_jobs;
    std::int32_t pending_window_transition;
    std::int32_t reserved;
};

struct oct_managed_frame_snapshot_t
{
    std::uint32_t version;
    std::uint32_t size;
    oct_managed_input_snapshot_t input;
    oct_managed_world_snapshot_t world;
};

using oct_managed_enqueue_command_fn = int (*)(const oct_managed_native_command_t* command);

struct oct_managed_native_api_t
{
    std::uint32_t version;
    std::uint32_t size;
    oct_managed_enqueue_command_fn enqueue_command;
};

inline constexpr std::uint32_t OCT_MANAGED_NATIVE_API_VERSION = 1;
inline constexpr std::uint32_t OCT_MANAGED_FRAME_SNAPSHOT_VERSION = 1;
inline constexpr std::uint32_t OCT_MANAGED_INPUT_FLAG_JUMP = 1u << 0u;
inline constexpr std::uint32_t OCT_MANAGED_INPUT_FLAG_SPRINT = 1u << 1u;
inline constexpr std::uint32_t OCT_MANAGED_INPUT_FLAG_FLY_MODE = 1u << 2u;
inline constexpr std::uint32_t OCT_MANAGED_INPUT_FLAG_PRIMARY = 1u << 3u;
inline constexpr std::uint32_t OCT_MANAGED_INPUT_FLAG_SECONDARY = 1u << 4u;
inline constexpr std::uint32_t OCT_MANAGED_NATIVE_COMMAND_FLAG_CRITICAL = 1u << 0u;

static_assert(sizeof(oct_managed_native_command_t) == 88);
static_assert(sizeof(oct_managed_input_snapshot_t) == 56);
static_assert(sizeof(oct_managed_world_snapshot_t) == 88);
static_assert(sizeof(oct_managed_frame_snapshot_t) == 152);
static_assert(sizeof(oct_managed_native_api_t) == 16);

static_assert(offsetof(oct_managed_native_command_t, kind) == 0);
static_assert(offsetof(oct_managed_native_command_t, flags) == 4);
static_assert(offsetof(oct_managed_native_command_t, request_id) == 8);
static_assert(offsetof(oct_managed_native_command_t, target_id) == 16);
static_assert(offsetof(oct_managed_native_command_t, a) == 24);
static_assert(offsetof(oct_managed_native_command_t, x) == 40);
static_assert(offsetof(oct_managed_native_command_t, payload0) == 72);
static_assert(offsetof(oct_managed_native_command_t, payload1) == 80);

static_assert(offsetof(oct_managed_input_snapshot_t, version) == 0);
static_assert(offsetof(oct_managed_input_snapshot_t, flags) == 8);
static_assert(offsetof(oct_managed_input_snapshot_t, move_x) == 16);
static_assert(offsetof(oct_managed_input_snapshot_t, camera_pitch) == 40);
static_assert(offsetof(oct_managed_input_snapshot_t, relative_mouse) == 48);

static_assert(offsetof(oct_managed_world_snapshot_t, version) == 0);
static_assert(offsetof(oct_managed_world_snapshot_t, frame_index) == 8);
static_assert(offsetof(oct_managed_world_snapshot_t, delta_seconds) == 24);
static_assert(offsetof(oct_managed_world_snapshot_t, second_of_day) == 40);
static_assert(offsetof(oct_managed_world_snapshot_t, player_block_x) == 44);
static_assert(offsetof(oct_managed_world_snapshot_t, pending_window_transition) == 76);

static_assert(offsetof(oct_managed_frame_snapshot_t, input) == 8);
static_assert(offsetof(oct_managed_frame_snapshot_t, world) == 64);

static_assert(offsetof(oct_managed_native_api_t, version) == 0);
static_assert(offsetof(oct_managed_native_api_t, size) == 4);
static_assert(offsetof(oct_managed_native_api_t, enqueue_command) == 8);
