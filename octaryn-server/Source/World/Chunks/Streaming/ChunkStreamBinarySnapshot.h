#pragma once

#include "ChunkColumnStream.h"

#include <vector>

bool write_chunk_stream_binary_snapshot(
    const char *stream_path,
    const octaryn_server_chunk_stream_snapshot_request &request,
    const std::vector<octaryn_server_chunk_stream_column> &columns,
    const std::vector<octaryn_server_chunk_stream_block> &blocks);
