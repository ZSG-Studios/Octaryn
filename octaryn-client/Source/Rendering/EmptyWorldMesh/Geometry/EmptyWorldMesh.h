#pragma once

#include "ChunkMeshPlan.h"
#include "ChunkView.h"
#include "JsonContracts.h"
#include "PresentationState.h"
#include "WorldMeshUpload.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct empty_world_terrain_column {
  int32_t height;
  uint16_t surface;
  uint16_t fill;
};

struct empty_world_dirty_column {
  int32_t chunk_x;
  int32_t chunk_z;
};

struct empty_world_retained_column {
  int32_t chunk_x;
  int32_t chunk_z;
};

struct empty_world_stream_mesh_batch_result {
  chunk_mesh_plan_summary summary{};
  size_t first_entry = 0u;
  size_t next_entry = 0u;
  size_t processed_entries = 0u;
  size_t remaining_entries = 0u;
  size_t build_columns = 0u;
  size_t clear_columns = 0u;
  bool complete = true;
};

empty_world_terrain_column empty_world_seed_column(int32_t world_x,
                                                   int32_t world_z);
uint32_t empty_world_block_atlas_layer(uint16_t block, uint32_t direction);
uint16_t
empty_world_generated_block(const octaryn_client_app::block_position_key &key);
uint16_t
empty_world_effective_block(const octaryn_client_app::block_lookup &overrides,
                            const octaryn_client_app::block_position_key &key);
void apply_empty_world_overrides_from_records(
    const std::vector<octaryn_client_app::world_block_record> &records,
    octaryn_client_app::block_lookup &overrides);
bool same_chunk_view(const chunk_view &left, const chunk_view &right);
chunk_view chunk_view_from_server_stream(
    const octaryn_client_app::server_chunk_stream_file &stream);
uint64_t hash_world_block_records(
    const std::vector<octaryn_client_app::world_block_record> &records);
void build_empty_world_mesh_frame(
    const chunk_view &current_view, const chunk_view &previous_chunk_view,
    const octaryn_client_app::block_lookup &overrides,
    world_mesh_upload_frame &mesh_frame);
void build_empty_world_mesh_frame_from_stream_columns(
    const octaryn_client_app::server_chunk_stream_file &stream,
    const std::vector<octaryn_client_app::server_chunk_stream_column_record>
        &selected_columns,
    const octaryn_client_app::block_lookup &overrides,
    const chunk_view &previous_chunk_view,
    const std::vector<empty_world_dirty_column> &dirty_columns,
    world_mesh_upload_frame &mesh_frame);
void build_empty_world_mesh_frame_from_stream_batch(
    const octaryn_client_app::server_chunk_stream_file &stream,
    const octaryn_client_app::block_lookup &overrides,
    const chunk_view &previous_chunk_view,
    const std::vector<empty_world_dirty_column> &dirty_columns,
    const std::vector<empty_world_retained_column> &retained_columns,
    size_t first_plan_entry, size_t max_plan_entries,
    world_mesh_upload_frame &mesh_frame,
    empty_world_stream_mesh_batch_result &batch_result);
