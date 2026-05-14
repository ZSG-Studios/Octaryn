#pragma once

#include <SDL3/SDL.h>

#include "world/block/block.h"

typedef struct persistence_world_save_metadata
{
    Uint32 version;
    bool save_exists;
    bool has_settings;
    bool has_lighting_tuning;
    bool has_world_time;
    bool has_player_data;
    bool has_world_data;
    Uint32 player_count;
    Uint32 chunk_override_count;
}
persistence_world_save_metadata_t;

typedef void (*persistence_set_block_t)(void* userdata, int bx, int by, int bz, block_t block);

bool persistence_init(const char* path);
void persistence_free(void);
void persistence_commit(void);
bool persistence_has_world_save(void);
bool persistence_get_world_save_metadata(persistence_world_save_metadata_t* metadata);
void persistence_set_player(int id, const void* data, int size);
bool persistence_get_player(int id, void* data, int size);
void persistence_set_settings(const void* data, int size);
bool persistence_get_settings(void* data, int size);
void persistence_set_lighting_tuning(const void* data, int size);
bool persistence_get_lighting_tuning(void* data, int size);
void persistence_set_world_time(const void* data, int size);
bool persistence_get_world_time(void* data, int size);
bool persistence_has_world_overrides(void);
void persistence_set_block(int cx, int cz, int bx, int by, int bz, block_t block);
void persistence_get_blocks(void* userdata, int cx, int cz, persistence_set_block_t function);
bool persistence_export_chunk_snapshot_lz4(int cx, int cz, void** data, int* size);
bool persistence_import_chunk_snapshot_lz4(const void* data, int size);
void persistence_free_export_buffer(void* data);
bool persistence_export_save_gzip(const char* path);
bool persistence_import_save_gzip(const char* path);
