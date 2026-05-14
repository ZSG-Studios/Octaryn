#include "BlockInteraction.h"

#include "EmptyWorldMesh.h"
#include "FileIO.h"
#include "HostCommands.h"
#include "JsonContracts.h"
#include "Log.h"

#include <glaze/glaze.hpp>

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace octaryn_client_app {

namespace {

constexpr glz::opts kJsonWriteOptions{.prettify = true};
constexpr float kBlockInteractionReachBlocks = 6.0f;
constexpr float kBlockInteractionRayStepBlocks = 0.05f;

block_position_key block_position_at(float x, float y, float z) {
  return block_position_key{
      static_cast<int32_t>(std::floor(x)),
      static_cast<int32_t>(std::floor(y)),
      static_cast<int32_t>(std::floor(z)),
  };
}

block_position_key fallback_adjacent_position(const block_position_key &hit) {
  return block_position_key{hit.x, hit.y + 1, hit.z};
}

block_position_key choose_lookup_adjacent_position(
    const block_lookup &lookup, const block_position_key &hit,
    const block_position_key &previous) {
  if (previous != hit && find_block(lookup, previous) == 0u) {
    return previous;
  }

  const block_position_key candidates[] = {
      block_position_key{hit.x, hit.y + 1, hit.z},
      block_position_key{hit.x + 1, hit.y, hit.z},
      block_position_key{hit.x - 1, hit.y, hit.z},
      block_position_key{hit.x, hit.y, hit.z + 1},
      block_position_key{hit.x, hit.y, hit.z - 1},
      block_position_key{hit.x, hit.y - 1, hit.z},
  };
  for (const block_position_key &candidate : candidates) {
    if (find_block(lookup, candidate) == 0u) {
      return candidate;
    }
  }
  return fallback_adjacent_position(hit);
}

block_position_key choose_empty_world_adjacent_position(
    const block_lookup &overrides, const block_position_key &hit,
    const block_position_key &previous) {
  if (previous != hit && empty_world_effective_block(overrides, previous) == 0u) {
    return previous;
  }

  const block_position_key candidates[] = {
      block_position_key{hit.x, hit.y + 1, hit.z},
      block_position_key{hit.x + 1, hit.y, hit.z},
      block_position_key{hit.x - 1, hit.y, hit.z},
      block_position_key{hit.x, hit.y, hit.z + 1},
      block_position_key{hit.x, hit.y, hit.z - 1},
      block_position_key{hit.x, hit.y - 1, hit.z},
  };
  for (const block_position_key &candidate : candidates) {
    if (empty_world_effective_block(overrides, candidate) == 0u) {
      return candidate;
    }
  }
  return fallback_adjacent_position(hit);
}

client_block_interaction_command_file make_block_interaction_command(
    uint64_t request_id, const block_position_key &edit, uint16_t block,
    const camera &camera, const block_position_key &hit) {
  return client_block_interaction_command_file{
      request_id,
      edit.x,
      edit.y,
      edit.z,
      block,
      camera.position[0],
      camera.position[1],
      camera.position[2],
      hit.x,
      hit.y,
      hit.z,
  };
}

octaryn_host_command make_logged_interaction_command(
    const client_block_interaction_command_file &source) {
  octaryn_host_command command{};
  command.version = 1u;
  command.size = OCTARYN_HOST_COMMAND_SIZE;
  command.kind = 1u;
  command.flags = kHostCommandCriticalFlag | kHostCommandClientInteractionFlag;
  command.request_id = source.requestId;
  command.a = source.editX;
  command.b = source.editY;
  command.c = source.editZ;
  command.d = source.block;
  command.x = source.cameraX;
  command.y = source.cameraY;
  command.z = source.cameraZ;
  command.x2 = static_cast<float>(source.hitX);
  command.y2 = static_cast<float>(source.hitY);
  command.z2 = static_cast<float>(source.hitZ);
  return command;
}

bool apply_client_block_interaction_edit(
    const client_block_interaction_command_file &command_file,
    std::vector<presentation_block> &world_blocks, block_lookup &lookup,
    uint64_t tick_id, bool preserve_air_edits) {
  const presentation_block update{
      command_file.editX,
      command_file.editY,
      command_file.editZ,
      command_file.block,
  };
  apply_local_block_record(world_blocks, update);

  const block_position_key key{update.x, update.y, update.z};
  if (update.block == 0u && !preserve_air_edits) {
    lookup.erase(key);
  } else {
    lookup[key] = update.block;
  }

  octaryn_replication_change change{};
  change.version = 1u;
  change.size = OCTARYN_REPLICATION_CHANGE_SIZE;
  change.change_kind = 1u;
  change.replication_id = tick_id;
  change.payload0 = pack_signed_pair(update.x, update.y);
  change.payload1 = pack_block(update.z, update.block);

  octaryn_server_snapshot_header snapshot{};
  snapshot.version = 1u;
  snapshot.size = OCTARYN_SERVER_SNAPSHOT_HEADER_SIZE;
  snapshot.change_count = 1u;
  snapshot.tick_id = tick_id;
  snapshot.changes_address =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&change));

  const int result = octaryn_client_apply_server_snapshot(&snapshot);
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_client_block_edit_apply result=%d edit=(%d,%d,%d,%u) "
                 "tick=%" PRIu64 "\n",
                 result, update.x, update.y, update.z,
                 static_cast<unsigned>(update.block), tick_id);
    std::fflush(g_log);
  }
  return result == 0;
}

} // namespace

client_block_raycast_hit
raycast_block_interaction(const camera &camera,
                          const block_lookup &lookup) {
  if (lookup.empty()) {
    return {};
  }

  float direction_x = 0.0f;
  float direction_y = 0.0f;
  float direction_z = 0.0f;
  camera_forward_vector(&camera, &direction_x, &direction_y,
                                       &direction_z);

  block_position_key previous = block_position_at(
      camera.position[0], camera.position[1], camera.position[2]);
  for (float distance = kBlockInteractionRayStepBlocks;
       distance <= kBlockInteractionReachBlocks;
       distance += kBlockInteractionRayStepBlocks) {
    const block_position_key current =
        block_position_at(camera.position[0] + direction_x * distance,
                          camera.position[1] + direction_y * distance,
                          camera.position[2] + direction_z * distance);
    const uint16_t block = find_block(lookup, current);
    if (block != 0u) {
      return client_block_raycast_hit{
          true,
          current,
          choose_lookup_adjacent_position(lookup, current, previous),
          block,
      };
    }

    previous = current;
  }

  return {};
}

client_block_raycast_hit
raycast_native_empty_world_interaction(const camera &camera,
                                       const block_lookup &overrides) {
  float direction_x = 0.0f;
  float direction_y = 0.0f;
  float direction_z = 0.0f;
  camera_forward_vector(&camera, &direction_x, &direction_y,
                                       &direction_z);

  block_position_key previous = block_position_at(
      camera.position[0], camera.position[1], camera.position[2]);
  for (float distance = kBlockInteractionRayStepBlocks;
       distance <= kBlockInteractionReachBlocks;
       distance += kBlockInteractionRayStepBlocks) {
    const block_position_key current =
        block_position_at(camera.position[0] + direction_x * distance,
                          camera.position[1] + direction_y * distance,
                          camera.position[2] + direction_z * distance);
    const uint16_t block = empty_world_effective_block(overrides, current);
    if (block != 0u) {
      return client_block_raycast_hit{
          true,
          current,
          choose_empty_world_adjacent_position(overrides, current, previous),
          block,
      };
    }

    previous = current;
  }

  return {};
}

bool write_block_interaction_intent(
    const octaryn_host_frame_snapshot &frame,
    const client_input_debug_state &input, const camera &camera,
    const client_block_raycast_hit &hit, uint16_t selected_place_block,
    std::vector<presentation_block> &world_blocks, block_lookup &lookup,
    bool preserve_air_edits) {
  const bool primary = (input.flags & kInputPrimaryFlag) != 0u;
  const bool secondary = (input.flags & kInputSecondaryFlag) != 0u;
  if (!primary && !secondary) {
    return true;
  }

  client_block_raycast_hit command_hit = hit;

  if (!command_hit.has_hit) {
    log_line("live_block_interaction_intent active=0 reason=raycast_miss");
    return true;
  }

  client_block_interaction_intent_file intent{};
  intent.frameIndex = frame.timing.frame_index;
  const uint64_t request_base = frame.timing.frame_index * 2u;
  if (secondary) {
    intent.commands.push_back(
        make_block_interaction_command(request_base + 1u, command_hit.adjacent,
                                       selected_place_block, camera,
                                       command_hit.hit));
  }
  if (primary) {
    intent.commands.push_back(make_block_interaction_command(
        request_base + 2u, command_hit.hit, 0u, camera, command_hit.hit));
  }

  for (const client_block_interaction_command_file &command_file :
       intent.commands) {
    octaryn_host_command command =
        make_logged_interaction_command(command_file);
    enqueue_command(&command);
    if (!apply_client_block_interaction_edit(command_file, world_blocks, lookup,
                                             frame.timing.frame_index + 2u,
                                             preserve_air_edits)) {
      log_line("live_client_block_edit_feedback=deferred reason=rejected");
    }
  }

  const char *path =
      std::getenv("OCTARYN_CLIENT_BLOCK_INTERACTION_INTENT_PATH");
  if (path == nullptr || path[0] == '\0') {
    log_line("live_block_interaction_intent_write=skipped reason=no_path");
    return true;
  }

  std::string output;
  const auto error = glz::write<kJsonWriteOptions>(intent, output);
  if (error) {
    log_line("live_block_interaction_intent_write=encode_failed");
    return false;
  }

  if (!write_text_file_atomic(std::filesystem::path(path), output,
                              "live_block_interaction_intent_write=failed")) {
    return false;
  }

  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_block_interaction_intent source=process_file path=%s "
                 "frame=%" PRIu64 " commands=%zu break=%d place=%d "
                 "hit=(%d,%d,%d,%u) adjacent=(%d,%d,%d)\n",
                 path, frame.timing.frame_index, intent.commands.size(),
                 primary ? 1 : 0, secondary ? 1 : 0, command_hit.hit.x,
                 command_hit.hit.y, command_hit.hit.z, command_hit.block,
                 command_hit.adjacent.x, command_hit.adjacent.y,
                 command_hit.adjacent.z);
    std::fflush(g_log);
  }
  return true;
}

} // namespace octaryn_client_app
