#include "ChunkColumnStream.h"

#include <glaze/glaze.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace octaryn::server::world::blocks {

struct block_interaction_command_file {
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

struct block_interaction_intent_file {
  int32_t version = 1;
  uint64_t frameIndex = 0u;
  std::vector<block_interaction_command_file> commands;
};

} // namespace octaryn::server::world::blocks

namespace {

constexpr glz::opts JsonReadOptions{.error_on_unknown_keys = false};
constexpr uint32_t MaxPendingCommands = 4096u;
constexpr uint32_t HostCommandKindSetBlock = 1u;

using octaryn::server::world::blocks::block_interaction_command_file;
using octaryn::server::world::blocks::block_interaction_intent_file;

bool read_text_file(const std::filesystem::path &path, std::string &text) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    return false;
  }

  text.assign(std::istreambuf_iterator<char>{input},
              std::istreambuf_iterator<char>{});
  return input.good() || input.eof();
}

bool is_supported(const block_interaction_command_file &command) {
  return command.requestId != 0u && std::isfinite(command.cameraX) &&
         std::isfinite(command.cameraY) && std::isfinite(command.cameraZ);
}

bool is_supported(const block_interaction_intent_file &file) {
  if (file.version != 1 || file.frameIndex == 0u ||
      file.commands.size() > MaxPendingCommands) {
    return false;
  }

  for (const auto &command : file.commands) {
    if (!is_supported(command)) {
      return false;
    }
  }
  return true;
}

octaryn_host_command to_host_command(
    const block_interaction_command_file &command) {
  return octaryn_host_command{
      .version = 1u,
      .size = OCTARYN_HOST_COMMAND_SIZE,
      .kind = HostCommandKindSetBlock,
      .flags = OCTARYN_HOST_COMMAND_CRITICAL_FLAG |
               OCTARYN_HOST_COMMAND_CLIENT_INTERACTION_FLAG,
      .request_id = command.requestId,
      .target_id = 0u,
      .a = command.editX,
      .b = command.editY,
      .c = command.editZ,
      .d = command.block,
      .x = command.cameraX,
      .y = command.cameraY,
      .z = command.cameraZ,
      .w = 0.0f,
      .x2 = static_cast<float>(command.hitX),
      .y2 = static_cast<float>(command.hitY),
      .z2 = static_cast<float>(command.hitZ),
      .w2 = 0.0f,
      .payload0 = 0u,
      .payload1 = 0u,
  };
}

uint32_t break_command_count(
    const std::vector<block_interaction_command_file> &commands) {
  uint32_t count = 0u;
  for (const auto &command : commands) {
    if (command.block == 0u) {
      ++count;
    }
  }
  return count;
}

octaryn_server_block_interaction_intent_result to_intent_result(
    const block_interaction_intent_file &file) {
  const uint32_t breaks = break_command_count(file.commands);
  return octaryn_server_block_interaction_intent_result{
      .frame_index = file.frameIndex,
      .command_count = static_cast<uint32_t>(file.commands.size()),
      .break_command_count = breaks,
      .place_command_count =
          static_cast<uint32_t>(file.commands.size()) - breaks,
  };
}

} // namespace

extern "C" {

int32_t octaryn_server_block_interaction_read_intent_file(
    const char *intent_path, octaryn_host_command *commands,
    uint32_t command_capacity,
    octaryn_server_block_interaction_intent_result *result) {
  if (intent_path == nullptr || intent_path[0] == '\0' || result == nullptr) {
    return -1;
  }

  const std::filesystem::path path{intent_path};
  if (!std::filesystem::exists(path)) {
    return 1;
  }

  std::string payload;
  if (!read_text_file(path, payload)) {
    return -2;
  }

  block_interaction_intent_file file{};
  if (glz::read<JsonReadOptions>(file, payload)) {
    return -3;
  }

  if (!is_supported(file)) {
    return -4;
  }

  if (file.commands.size() > command_capacity) {
    *result = to_intent_result(file);
    return -5;
  }
  if (!file.commands.empty() && commands == nullptr) {
    return -1;
  }

  for (size_t index = 0; index < file.commands.size(); ++index) {
    commands[index] = to_host_command(file.commands[index]);
  }
  *result = to_intent_result(file);
  return 0;
}

}
