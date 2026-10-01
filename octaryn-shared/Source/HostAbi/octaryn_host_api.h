#pragma once

#include <stdint.h>

#include "octaryn_shared_abi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Domain API table identifiers for octaryn_host_api_query_fn. */
#define OCTARYN_HOST_API_TIME 1u
#define OCTARYN_HOST_API_DIAGNOSTICS 2u
#define OCTARYN_HOST_API_PHYSICS 3u
#define OCTARYN_HOST_API_WORLD 4u
#define OCTARYN_HOST_API_INPUT 5u
#define OCTARYN_HOST_API_SCHEDULING 6u
#define OCTARYN_HOST_API_AUDIO 7u
#define OCTARYN_HOST_API_UI 8u
#define OCTARYN_HOST_API_REPLICATION 9u

#define OCTARYN_HOST_TIME_API_VERSION 1u
#define OCTARYN_HOST_DIAGNOSTICS_API_VERSION 1u
#define OCTARYN_HOST_PHYSICS_API_VERSION 3u
#define OCTARYN_HOST_WORLD_API_VERSION 1u
#define OCTARYN_HOST_INPUT_API_VERSION 1u
#define OCTARYN_HOST_SCHEDULING_API_VERSION 1u
#define OCTARYN_HOST_AUDIO_API_VERSION 1u
#define OCTARYN_HOST_UI_API_VERSION 1u
#define OCTARYN_HOST_REPLICATION_API_VERSION 3u

#define OCTARYN_HOST_TIME_API_SIZE 32u
#define OCTARYN_HOST_DIAGNOSTICS_API_SIZE 16u
#define OCTARYN_HOST_PHYSICS_API_SIZE 32u
#define OCTARYN_HOST_WORLD_API_SIZE 32u
#define OCTARYN_HOST_INPUT_API_SIZE 16u
#define OCTARYN_HOST_SCHEDULING_API_SIZE 16u
#define OCTARYN_HOST_AUDIO_API_SIZE 16u
#define OCTARYN_HOST_UI_API_SIZE 24u
#define OCTARYN_HOST_REPLICATION_API_SIZE 48u
#define OCTARYN_HOST_RAYCAST_HIT_SIZE 40u
#define OCTARYN_HOST_SPAWN_POSE_SIZE 24u
#define OCTARYN_HOST_CHARACTER_INPUT_SIZE 48u
#define OCTARYN_HOST_CHARACTER_STATE_SIZE 48u

typedef enum octaryn_host_log_level {
    OCTARYN_HOST_LOG_TRACE = 0,
    OCTARYN_HOST_LOG_DEBUG = 1,
    OCTARYN_HOST_LOG_INFO = 2,
    OCTARYN_HOST_LOG_WARNING = 3,
    OCTARYN_HOST_LOG_ERROR = 4
} octaryn_host_log_level;

typedef double (OCTARYN_ABI_CALL* octaryn_host_time_now_seconds_fn)(void);
typedef uint64_t (OCTARYN_ABI_CALL* octaryn_host_time_tick_id_fn)(void);
typedef double (OCTARYN_ABI_CALL* octaryn_host_time_tick_rate_fn)(void);
typedef void (OCTARYN_ABI_CALL* octaryn_host_log_write_fn)(uint32_t level, const char* message_utf8);

typedef struct octaryn_host_time_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_time_now_seconds_fn now_seconds;
    octaryn_host_time_tick_id_fn tick_id;
    octaryn_host_time_tick_rate_fn tick_rate;
} octaryn_host_time_api;

typedef struct octaryn_host_diagnostics_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_log_write_fn log_write;
} octaryn_host_diagnostics_api;

/* Physics domain: queries against the host collision world. */
typedef struct octaryn_host_raycast_hit {
    uint32_t hit;
    uint32_t material_id;
    float point_x;
    float point_y;
    float point_z;
    float normal_x;
    float normal_y;
    float normal_z;
    float distance;
    uint32_t triangle_index;
} octaryn_host_raycast_hit;

/* Returns 0 on hit, 1 on miss, <0 on error. Direction need not be normalized. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_raycast_fn)(
    float origin_x, float origin_y, float origin_z,
    float direction_x, float direction_y, float direction_z,
    float max_distance, octaryn_host_raycast_hit* out_hit);

/* Kinematic character mover, mirroring the shared CharacterMotion library. */
typedef struct octaryn_host_character_input {
    uint32_t flags;
    uint32_t controller;
    float move_x;
    float move_y;
    float move_z;
    float camera_x;
    float camera_y;
    float camera_z;
    float camera_pitch;
    float camera_yaw;
    int32_t relative_mouse;
    uint32_t reserved;
} octaryn_host_character_input;

typedef struct octaryn_host_character_state {
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    float velocity_x;
    float velocity_y;
    float velocity_z;
    uint32_t is_on_ground;
    uint32_t control_mode;
    uint32_t jump_held;
    uint32_t reserved;
} octaryn_host_character_state;

/* Steps the character state against the collision world; 0 on success. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_move_character_fn)(
    const octaryn_host_character_input* input, double delta_seconds,
    octaryn_host_character_state* state);

/* Ballistic dropped-item state; position is the collision capsule centre. */
typedef struct octaryn_host_world_item_state {
    float x;
    float y;
    float z;
    float velocity_x;
    float velocity_y;
    float velocity_z;
    uint32_t grounded;
    uint32_t sleeping;
    float sleep_timer;
    uint32_t reserved;
} octaryn_host_world_item_state;

/* Steps one dropped item with swept motion, restitution, friction, and
   settle-to-sleep; 0 on success. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_step_world_item_fn)(
    octaryn_host_world_item_state* state, double delta_seconds);

typedef struct octaryn_host_physics_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_raycast_fn raycast;
    /* Present from table version 2 / size 24 onward. */
    octaryn_host_move_character_fn move_character;
    /* Present from table version 3 / size 32 onward. */
    octaryn_host_step_world_item_fn step_world_item;
} octaryn_host_physics_api;

/* World domain: static facts about the active world. */
typedef struct octaryn_host_spawn_pose {
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    uint32_t valid;
} octaryn_host_spawn_pose;

typedef int (OCTARYN_ABI_CALL* octaryn_host_spawn_pose_fn)(octaryn_host_spawn_pose* out_pose);
typedef uint64_t (OCTARYN_ABI_CALL* octaryn_host_world_triangle_count_fn)(void);
typedef int (OCTARYN_ABI_CALL* octaryn_host_world_is_active_fn)(void);

typedef struct octaryn_host_world_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_spawn_pose_fn spawn_pose;
    octaryn_host_world_triangle_count_fn triangle_count;
    octaryn_host_world_is_active_fn is_active;
} octaryn_host_world_api;

/* Input domain: latest authoritative input snapshot, reuses the ABI struct. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_poll_input_fn)(octaryn_host_input_snapshot* out_snapshot);

typedef struct octaryn_host_input_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_poll_input_fn poll_input;
} octaryn_host_input_api;

/* Scheduling domain: module work on host threads. worker=0 main, 1 pool. */
typedef void (OCTARYN_ABI_CALL* octaryn_host_work_callback_fn)(void* user);
typedef int (OCTARYN_ABI_CALL* octaryn_host_submit_work_fn)(
    uint32_t worker, octaryn_host_work_callback_fn callback, void* user);

typedef struct octaryn_host_scheduling_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_submit_work_fn submit_work;
} octaryn_host_scheduling_api;

/* Audio domain: play a module-declared action sound, hashed asset id. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_play_sound_fn)(
    uint64_t asset_id_hash, float volume,
    float position_x, float position_y, float position_z);

typedef struct octaryn_host_audio_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_play_sound_fn play_action_sound;
} octaryn_host_audio_api;

/* UI domain: module-declared notifications and polled UI actions. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_show_notification_fn)(const char* text_utf8);
/* Writes the next pending UI action id into buffer; 0 when one was written,
   1 when the queue is empty, <0 on error. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_poll_ui_action_fn)(char* buffer_utf8, uint32_t capacity);

typedef struct octaryn_host_ui_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_show_notification_fn show_notification;
    octaryn_host_poll_ui_action_fn poll_ui_action;
} octaryn_host_ui_api;

/* Replication domain: publish changes and messages on the authority. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_publish_change_fn)(const octaryn_replication_change* change);
typedef int (OCTARYN_ABI_CALL* octaryn_host_send_message_fn)(
    uint64_t replication_id, const void* payload, uint32_t payload_bytes);
typedef int (OCTARYN_ABI_CALL* octaryn_host_replication_capacity_fn)(void);
typedef struct octaryn_host_world_item_pose {
    uint64_t entity_id, generation, source_tick;
    float x, y, z, velocity_x, velocity_y, velocity_z;
    uint32_t item_id, count, flags, reserved;
} octaryn_host_world_item_pose;
typedef int (OCTARYN_ABI_CALL* octaryn_host_publish_world_item_fn)(const octaryn_host_world_item_pose* pose);

typedef struct octaryn_host_replication_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_publish_change_fn publish_change;
    octaryn_host_send_message_fn send_message;
    octaryn_host_replication_capacity_fn available_change_capacity;
    octaryn_host_replication_capacity_fn available_world_item_capacity;
    octaryn_host_publish_world_item_fn publish_world_item;
} octaryn_host_replication_api;

#ifdef __cplusplus
}
#endif
