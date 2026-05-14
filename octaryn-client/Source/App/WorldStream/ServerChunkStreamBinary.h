#pragma once

#include "JsonContracts.h"
#include "WorldStream.h"

#include <filesystem>

bool load_server_chunk_stream_binary_file(
    const std::filesystem::path &json_stream_path,
    octaryn_client_app::server_chunk_stream_file &stream);
