#pragma once
#include "WorldStream.h"

void validate_preloaded_halo();
void validate_stream_request_activation(const std::filesystem::path& snapshot_path);
void validate_private_halo(const octaryn::client::world_presentation::WorldStream& stream,
    const octaryn::client::world_presentation::StreamColumn& column);
