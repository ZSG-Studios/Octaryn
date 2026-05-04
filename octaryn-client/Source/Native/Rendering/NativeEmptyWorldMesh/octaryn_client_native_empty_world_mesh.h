#pragma once

#include "octaryn_client_app_json_files.h"
#include "octaryn_client_app_presentation_state.h"
#include "octaryn_client_chunk_view.h"
#include "octaryn_client_world_mesh_upload.h"

#include <cstdint>
#include <vector>

uint16_t native_empty_generated_block(
    const octaryn_client_app::block_position_key &key);
uint16_t native_empty_effective_block(
    const octaryn_client_app::block_lookup &overrides,
    const octaryn_client_app::block_position_key &key);
void apply_native_empty_overrides_from_records(
    const std::vector<octaryn_client_app::world_block_record> &records,
    octaryn_client_app::block_lookup &overrides);
bool same_chunk_view(const octaryn_client_chunk_view &left,
                     const octaryn_client_chunk_view &right);
octaryn_client_chunk_view chunk_view_from_server_stream(
    const octaryn_client_app::server_chunk_stream_file &stream);
uint64_t hash_world_block_records(
    const std::vector<octaryn_client_app::world_block_record> &records);
void build_native_empty_world_mesh_frame(
    const octaryn_client_chunk_view &chunk_view,
    const octaryn_client_chunk_view &previous_chunk_view,
    const octaryn_client_app::block_lookup &overrides,
    world_mesh_upload_frame &mesh_frame);
void build_native_empty_world_mesh_frame_from_stream(
    const octaryn_client_app::server_chunk_stream_file &stream,
    const octaryn_client_app::block_lookup &overrides,
    const octaryn_client_chunk_view &previous_chunk_view,
    world_mesh_upload_frame &mesh_frame);
