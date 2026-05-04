#pragma once

#include "JsonFiles.h"
#include "PresentationState.h"
#include "ChunkView.h"
#include "octaryn_client_world_mesh_upload.h"

#include <cstdint>
#include <vector>

uint16_t
empty_world_generated_block(const octaryn_client_app::block_position_key &key);
uint16_t
empty_world_effective_block(const octaryn_client_app::block_lookup &overrides,
                            const octaryn_client_app::block_position_key &key);
void apply_empty_world_overrides_from_records(
    const std::vector<octaryn_client_app::world_block_record> &records,
    octaryn_client_app::block_lookup &overrides);
bool same_chunk_view(const chunk_view &left,
                     const chunk_view &right);
chunk_view chunk_view_from_server_stream(
    const octaryn_client_app::server_chunk_stream_file &stream);
uint64_t hash_world_block_records(
    const std::vector<octaryn_client_app::world_block_record> &records);
void build_empty_world_mesh_frame(
    const chunk_view &current_view,
    const chunk_view &previous_chunk_view,
    const octaryn_client_app::block_lookup &overrides,
    world_mesh_upload_frame &mesh_frame);
void build_empty_world_mesh_frame_from_stream(
    const octaryn_client_app::server_chunk_stream_file &stream,
    const octaryn_client_app::block_lookup &overrides,
    const chunk_view &previous_chunk_view,
    world_mesh_upload_frame &mesh_frame);
