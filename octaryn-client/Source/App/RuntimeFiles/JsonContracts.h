#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace octaryn_client_app {

struct world_block_record {
  int32_t x;
  int32_t y;
  int32_t z;
  uint16_t block;
};

struct world_block_file {
  int32_t version = 0;
  std::vector<world_block_record> blocks;
};

struct server_chunk_stream_column_record {
  int32_t chunkX;
  int32_t chunkZ;
  int32_t originX;
  int32_t originZ;
  uint32_t blockOffset;
  uint32_t blockCount;
};

struct server_chunk_stream_file {
  int32_t version = 0;
  uint64_t epoch = 0u;
  std::string source;
  int32_t centerChunkX = 0;
  int32_t centerChunkZ = 0;
  uint32_t radius = 0u;
  uint64_t worldSeed = 0u;
  uint64_t worldTimeDayIndex = 0u;
  uint32_t worldTimeSecondOfDay = 0u;
  double worldTimeTotalSeconds = 0.0;
  float worldTimeDayFraction = 0.5f;
  std::vector<server_chunk_stream_column_record> columns;
  std::vector<world_block_record> blocks;
};

struct graphics_shader_metadata_file {
  uint32_t samplers = 0u;
  uint32_t storage_textures = 0u;
  uint32_t storage_buffers = 0u;
  uint32_t uniform_buffers = 0u;
};

struct compute_shader_metadata_file {
  uint32_t samplers = 0u;
  uint32_t readonly_storage_textures = 0u;
  uint32_t readonly_storage_buffers = 0u;
  uint32_t readwrite_storage_textures = 0u;
  uint32_t readwrite_storage_buffers = 0u;
  uint32_t uniform_buffers = 0u;
  uint32_t threadcount_x = 0u;
  uint32_t threadcount_y = 0u;
  uint32_t threadcount_z = 0u;
};

struct client_chunk_view_intent_file {
  int32_t version = 1;
  uint64_t epoch = 0u;
  int32_t centerChunkX = 0;
  int32_t centerChunkZ = 0;
  uint32_t radius = 0u;
  bool hasPreviousWindow = false;
  int32_t previousCenterChunkX = 0;
  int32_t previousCenterChunkZ = 0;
  uint32_t previousRadius = 0u;
};

struct client_player_input_intent_file {
  int32_t version = 1;
  uint64_t frameIndex = 0u;
  double deltaSeconds = 0.0;
  uint32_t flags = 0u;
  uint32_t controller = 0u;
  float moveX = 0.0f;
  float moveY = 0.0f;
  float moveZ = 0.0f;
  float cameraX = 0.0f;
  float cameraY = 0.0f;
  float cameraZ = 0.0f;
  float cameraPitch = 0.0f;
  float cameraYaw = 0.0f;
  int32_t relativeMouse = 0;
};

struct client_block_interaction_command_file {
  uint64_t requestId = 0u;
  int32_t editX = 0;
  int32_t editY = 0;
  int32_t editZ = 0;
  uint16_t block = 0u;
  float cameraX = 0.0f;
  float cameraY = 0.0f;
  float cameraZ = 0.0f;
  int32_t hitX = 0;
  int32_t hitY = 0;
  int32_t hitZ = 0;
};

struct client_block_interaction_intent_file {
  int32_t version = 1;
  uint64_t frameIndex = 0u;
  std::vector<client_block_interaction_command_file> commands;
};

struct client_world_time_intent_file {
  int32_t version = 1;
  int32_t speedIndex = 2;
  double speedMultiplier = 1.0;
};

} // namespace octaryn_client_app
