#pragma once

#include <stdint.h>

#include "octaryn_shared_abi_types.h"
#include "octaryn_residency_api.h"

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
#define OCTARYN_HOST_API_CONTENT 10u
#define OCTARYN_HOST_API_SCENE 11u
#define OCTARYN_HOST_API_GRAPHICS 12u
#define OCTARYN_HOST_API_APPLICATION 13u
#define OCTARYN_HOST_API_TRANSITION 14u
#define OCTARYN_HOST_TRANSITION_API_VERSION 1u
#define OCTARYN_HOST_TRANSITION_API_SIZE 40u
#define OCTARYN_TRANSITION_QUEUED 0u
#define OCTARYN_TRANSITION_LOADING 1u
#define OCTARYN_TRANSITION_COMPLETED 2u
#define OCTARYN_TRANSITION_FAILED 3u
typedef struct octaryn_host_transition_pose {double x,y,z;float yaw,pitch;} octaryn_host_transition_pose;
typedef struct octaryn_host_transition_view {
    octaryn_host_transition_pose pose;
    uint32_t enabled,reserved;
    char asset_id[256];
} octaryn_host_transition_view;
typedef int (OCTARYN_ABI_CALL* octaryn_host_transition_view_fn)(octaryn_host_transition_view*);
typedef int (OCTARYN_ABI_CALL* octaryn_host_transition_begin_fn)(const char*,const char*,const octaryn_host_transition_pose*,uint64_t*);
typedef int (OCTARYN_ABI_CALL* octaryn_host_transition_status_fn)(uint64_t,uint32_t*,char*,uint32_t);
typedef int (OCTARYN_ABI_CALL* octaryn_host_transition_cancel_fn)(uint64_t);
typedef struct octaryn_host_transition_api {
    uint32_t version,size;
    octaryn_host_transition_view_fn read_view;
    octaryn_host_transition_begin_fn begin;
    octaryn_host_transition_status_fn status;
    octaryn_host_transition_cancel_fn cancel;
} octaryn_host_transition_api;
#define OCTARYN_HOST_APPLICATION_API_VERSION 1u
#define OCTARYN_HOST_APPLICATION_API_SIZE 16u
/* Zero accepts normal client-loop shutdown; nonzero leaves the host running. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_application_exit_fn)(void);
typedef struct octaryn_host_application_api {
    uint32_t version, size;
    octaryn_host_application_exit_fn request_exit;
} octaryn_host_application_api;
#define OCTARYN_HOST_GRAPHICS_API_VERSION 1u
#define OCTARYN_HOST_GRAPHICS_API_SIZE 24u
#define OCTARYN_HOST_GRAPHICS_SETTINGS_SIZE 88u
#define OCTARYN_GRAPHICS_FULLSCREEN (1u << 0)
#define OCTARYN_GRAPHICS_SHARPENING (1u << 1)
#define OCTARYN_GRAPHICS_DYNAMIC_RESOLUTION (1u << 2)
#define OCTARYN_GRAPHICS_RAY_TRACING (1u << 3)
#define OCTARYN_GRAPHICS_PBR (1u << 4)
#define OCTARYN_GRAPHICS_POM (1u << 5)
#define OCTARYN_GRAPHICS_FOG (1u << 6)
#define OCTARYN_GRAPHICS_CLOUDS (1u << 7)
#define OCTARYN_GRAPHICS_SKY_GRADIENT (1u << 8)
#define OCTARYN_GRAPHICS_STARS (1u << 9)
#define OCTARYN_GRAPHICS_SUN (1u << 10)
#define OCTARYN_GRAPHICS_MOON (1u << 11)
#define OCTARYN_GRAPHICS_RAY_AVAILABLE 1u
#define OCTARYN_GRAPHICS_APPLIED 0
#define OCTARYN_GRAPHICS_APPLIED_NOT_PERSISTED 1
#define OCTARYN_GRAPHICS_REJECTED 2
#define OCTARYN_GRAPHICS_UNAVAILABLE 3
/* Actual dimensions/capabilities are readback, not writable preferences. */
typedef struct octaryn_host_graphics_settings {
    uint32_t version, size, flags, capabilities;
    uint32_t window_width, window_height, present_mode, frame_cap_fps, upscaler_mode;
    uint32_t reflection_quality, shadow_quality, shadow_distance, reflection_distance, fsr_target_fps;
    float fsr_sharpness, fsr_render_scale, fsr_min_scale, fsr_max_scale;
    uint32_t render_width, render_height, display_width, display_height;
} octaryn_host_graphics_settings;
typedef int (OCTARYN_ABI_CALL* octaryn_host_graphics_get_fn)(octaryn_host_graphics_settings* settings);
typedef int (OCTARYN_ABI_CALL* octaryn_host_graphics_apply_fn)(const octaryn_host_graphics_settings* settings, uint32_t persist);
typedef struct octaryn_host_graphics_api {
    uint32_t version, size;
    octaryn_host_graphics_get_fn get;
    octaryn_host_graphics_apply_fn apply;
} octaryn_host_graphics_api;
#define OCTARYN_HOST_SCENE_API_VERSION 1u
#define OCTARYN_HOST_SCENE_API_SIZE 48u
#define OCTARYN_HOST_SCENE_MAX_TICKETS 8u
#define OCTARYN_SCENE_QUEUED 0u
#define OCTARYN_SCENE_RUNNING 1u
#define OCTARYN_SCENE_CPU_PREPARED 2u
#define OCTARYN_SCENE_FAILED 3u
#define OCTARYN_SCENE_CANCELED 4u
#define OCTARYN_SCENE_UNPUBLISHED 0u
#define OCTARYN_HOST_CONTENT_API_VERSION 1u
#define OCTARYN_HOST_CONTENT_API_SIZE 24u
#define OCTARYN_HOST_CONTENT_MAX_READ_BYTES (4u * 1024u * 1024u)

#define OCTARYN_HOST_TIME_API_VERSION 1u
#define OCTARYN_HOST_DIAGNOSTICS_API_VERSION 1u
#define OCTARYN_HOST_PHYSICS_API_VERSION 3u
#define OCTARYN_HOST_WORLD_API_VERSION 1u
#define OCTARYN_HOST_INPUT_API_VERSION 1u
#define OCTARYN_HOST_SCHEDULING_API_VERSION 1u
#define OCTARYN_HOST_AUDIO_API_VERSION 2u
#define OCTARYN_HOST_UI_API_VERSION 2u
#define OCTARYN_HOST_REPLICATION_API_VERSION 3u

#define OCTARYN_HOST_TIME_API_SIZE 32u
#define OCTARYN_HOST_DIAGNOSTICS_API_SIZE 16u
#define OCTARYN_HOST_PHYSICS_API_SIZE 32u
#define OCTARYN_HOST_WORLD_API_SIZE 32u
#define OCTARYN_HOST_INPUT_API_SIZE 16u
#define OCTARYN_HOST_SCHEDULING_API_SIZE 16u
#define OCTARYN_HOST_AUDIO_API_SIZE 56u
#define OCTARYN_HOST_UI_API_SIZE 40u
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

/* Host resolves module/declaration IDs; never accept paths. Null buffer with
   zero capacity queries length. 0=success, 1=short buffer, <0=unavailable.
   Each read is bounded, revalidated and copies into caller-owned memory. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_content_read_fn)(
    const char* module_id_utf8, const char* content_id_utf8,
    void* buffer, uint32_t capacity, uint32_t* out_bytes);
typedef struct octaryn_host_content_api {
    uint32_t version, size;
    octaryn_host_content_read_fn read_data;
    uint32_t maximum_read_bytes, reserved;
} octaryn_host_content_api;

typedef struct octaryn_host_scene_ticket { uint64_t id, generation; } octaryn_host_scene_ticket;
typedef struct octaryn_host_scene_progress {
    uint32_t preparation, publication;
    uint64_t completed, total, retained_bytes;
} octaryn_host_scene_progress;
/* IDs resolve through validated declarations; backend tickets are unique
   across activations. Release invalidates immediately, retires workers later.
   CpuPrepared verifies metadata/resources and never promises GPU residency. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_scene_begin_fn)(
    const char* module_id_utf8, const char* asset_id_utf8, octaryn_host_scene_ticket* ticket);
typedef int (OCTARYN_ABI_CALL* octaryn_host_scene_query_fn)(
    const char* module_id_utf8, const octaryn_host_scene_ticket* ticket, octaryn_host_scene_progress* progress);
typedef int (OCTARYN_ABI_CALL* octaryn_host_scene_ticket_fn)(
    const char* module_id_utf8, const octaryn_host_scene_ticket* ticket);
typedef struct octaryn_host_scene_api {
    uint32_t version, size;
    octaryn_host_scene_begin_fn begin_prepare;
    octaryn_host_scene_query_fn query;
    octaryn_host_scene_ticket_fn cancel, release;
    uint32_t maximum_tickets, reserved;
} octaryn_host_scene_api;

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

/* Audio v1 prefix: built-in nonspatial action IDs 0..3.
   v2 appends bounded copied PCM16 handles; samples never represent file paths.
   PCM flags: 1=loop, 2=nonspatial. Source attenuation belongs to the caller. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_play_sound_fn)(
    uint64_t asset_id_hash, float volume,
    float position_x, float position_y, float position_z);

typedef struct octaryn_host_audio_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_play_sound_fn play_action_sound;
    int (OCTARYN_ABI_CALL* register_pcm16)(const uint8_t*,uint32_t,uint32_t,uint32_t,uint64_t*);
    int (OCTARYN_ABI_CALL* play_clip)(uint64_t,float,uint32_t,float,float,float,uint64_t*);
    int (OCTARYN_ABI_CALL* stop_voice)(uint64_t);
    int (OCTARYN_ABI_CALL* release_clip)(uint64_t);
    int (OCTARYN_ABI_CALL* query_voice)(uint64_t,uint32_t*);
} octaryn_host_audio_api;

/* UI domain: module-declared notifications and polled UI actions. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_show_notification_fn)(const char* text_utf8);
/* Writes the next pending UI action id into buffer; 0 when one was written,
   1 when the queue is empty, <0 on error. */
typedef int (OCTARYN_ABI_CALL* octaryn_host_poll_ui_action_fn)(char* buffer_utf8, uint32_t capacity);
typedef int (OCTARYN_ABI_CALL* octaryn_host_present_screen_fn)(const char* declaration_json, const char* fields_json);
typedef int (OCTARYN_ABI_CALL* octaryn_host_hide_screen_fn)(const char* screen_id);

typedef struct octaryn_host_ui_api {
    uint32_t version;
    uint32_t size;
    octaryn_host_show_notification_fn show_notification;
    octaryn_host_poll_ui_action_fn poll_ui_action;
    octaryn_host_present_screen_fn present_screen;
    octaryn_host_hide_screen_fn hide_screen;
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
