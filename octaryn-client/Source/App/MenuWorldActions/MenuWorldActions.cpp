#include "MenuWorldActions.h"

#include "ChunkView.h"
#include "Environment.h"
#include "FileIO.h"
#include "Input.h"
#include "Log.h"
#include "WorldIntents.h"

#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <system_error>
#include <string>

namespace octaryn_client_app {
namespace {

constexpr double kStartupDeltaSeconds = 1.0 / 60.0;
constexpr const char *kWorldMetadataFileName = "world.json";

bool write_world_metadata(const std::filesystem::path &world_root,
                          uint32_t world_slot, const char *world_name,
                          bool replace_existing) {
  const std::filesystem::path metadata_path = world_root / kWorldMetadataFileName;
  if (!replace_existing && std::filesystem::exists(metadata_path)) {
    return true;
  }

  const std::string name =
      world_name != nullptr && world_name[0] != '\0'
          ? std::string(world_name)
          : "World " + std::to_string(world_slot + 1u);
  const std::string payload =
      "{\n"
      "  \"version\": 1,\n"
      "  \"name\": \"" +
      name +
      "\",\n"
      "  \"slot\": " +
      std::to_string(world_slot) +
      "\n"
      "}\n";
  return write_text_file_atomic(metadata_path, payload,
                                "singleplayer_world_metadata_write=failed");
}

bool selected_world_exists(const std::filesystem::path &world_root) {
  return std::filesystem::exists(world_root / kWorldMetadataFileName);
}

std::filesystem::path singleplayer_world_root(uint32_t world_slot) {
  std::error_code error;
  const std::filesystem::path root = std::filesystem::current_path(error);
  if (error) {
    return {};
  }
  return root / "saves" / "singleplayer" /
         ("world" + std::to_string(world_slot + 1u));
}

bool remove_world_save(uint32_t world_slot) {
  std::error_code error;
  const std::filesystem::path root = singleplayer_world_root(world_slot);
  if (root.empty() || !std::filesystem::exists(root)) {
    return false;
  }
  std::filesystem::remove_all(root, error);
  return !error;
}

bool valid_port_text(const char *text) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }
  uint32_t value = 0u;
  for (uint32_t index = 0u; text[index] != '\0'; ++index) {
    if (text[index] < '0' || text[index] > '9') {
      return false;
    }
    value = value * 10u + static_cast<uint32_t>(text[index] - '0');
  }
  return value > 0u && value <= 65535u;
}

bool valid_address_text(const char *text) {
  return text != nullptr && text[0] != '\0';
}

bool is_local_address(const char *text) {
  if (text == nullptr) {
    return false;
  }
  const std::string address{text};
  return address == "127.0.0.1" || address == "localhost" || address == "0.0.0.0";
}

std::filesystem::path workspace_root() {
  std::error_code error;
  std::filesystem::path cursor = std::filesystem::current_path(error);
  if (error) {
    return {};
  }
  for (int depth = 0; depth < 8; ++depth) {
    if (std::filesystem::exists(cursor / "REQUESTS.md") &&
        std::filesystem::exists(cursor / "octaryn-client")) {
      return cursor;
    }
    if (!cursor.has_parent_path()) {
      break;
    }
    cursor = cursor.parent_path();
  }
  return {};
}

bool attach_local_file_bridge(singleplayer_server_session &session) {
  const std::filesystem::path root = workspace_root();
  if (root.empty()) {
    return false;
  }
  const std::filesystem::path bridge = root / "logs" / "server" / "dedicated-run";
  const std::filesystem::path save = root / "saves" / "server" / "dedicated";
  std::error_code error;
  std::filesystem::create_directories(bridge, error);
  if (error) {
    return false;
  }
  std::filesystem::create_directories(save, error);
  if (error) {
    return false;
  }

  session = {};
  session.enabled = true;
  session.running = false;
  session.root = bridge;
  session.world_root = save;
  session.world_blocks_path = save / "world_blocks.json";
  session.chunk_view_intent_path = bridge / "chunk_view_intent.json";
  session.chunk_stream_path = bridge / "chunk_stream.json";
  session.player_input_intent_path = bridge / "player_input_intent.json";
  session.block_interaction_intent_path = bridge / "block_interaction_intent.json";
  session.world_time_intent_path = bridge / "world_time_intent.json";
  session.server_log_path = bridge / "server_live.log";

  return set_process_env("OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH",
                         session.chunk_stream_path) &&
         set_process_env("OCTARYN_CLIENT_CHUNK_VIEW_INTENT_PATH",
                         session.chunk_view_intent_path) &&
         set_process_env("OCTARYN_CLIENT_PLAYER_INPUT_INTENT_PATH",
                         session.player_input_intent_path) &&
         set_process_env("OCTARYN_CLIENT_BLOCK_INTERACTION_INTENT_PATH",
                         session.block_interaction_intent_path);
}

bool clear_client_file_bridge() {
  return set_process_env_text("OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH", "") &&
         set_process_env_text("OCTARYN_CLIENT_CHUNK_VIEW_INTENT_PATH", "") &&
         set_process_env_text("OCTARYN_CLIENT_PLAYER_INPUT_INTENT_PATH", "") &&
         set_process_env_text("OCTARYN_CLIENT_BLOCK_INTERACTION_INTENT_PATH", "");
}

} // namespace

void refresh_singleplayer_world_slots(runtime_controls &controls) {
  uint32_t exists_mask = 0u;
  for (uint32_t slot = 0u; slot < 3u; ++slot) {
    const std::filesystem::path root = singleplayer_world_root(slot);
    if (!root.empty() && selected_world_exists(root)) {
      exists_mask |= 1u << slot;
    }
  }
  controls.display_menu.world_exists_mask = exists_mask;
}

menu_action_result run_menu_action(
    singleplayer_server_session &server_session, bool game_modules_disabled,
    const camera &camera, int render_distance,
    const client_world_time_controls &world_time_controls, uint32_t action,
    uint32_t world_slot, const char *server_address, const char *server_port,
    const char *world_name, uint32_t &menu_status_code, int &result) {
  if (action == DISPLAY_MENU_ACTION_NONE) {
    return MENU_ACTION_RESULT_IGNORED;
  }

  if (action == DISPLAY_MENU_ACTION_DISCONNECT_SESSION) {
    stop_singleplayer_server(server_session);
    server_session = {};
    const bool bridge_cleared = clear_client_file_bridge();
    if (g_log != nullptr) {
      std::fprintf(g_log,
                   "live_menu_action_result action=%" PRIu32
                   " status=returned_to_main_menu bridge_cleared=%d\n",
                   action, bridge_cleared ? 1 : 0);
      std::fflush(g_log);
    }
    if (!bridge_cleared) {
      menu_status_code = DISPLAY_MENU_STATUS_FAILED;
      result = -9;
      return MENU_ACTION_RESULT_FATAL;
    }
    menu_status_code = DISPLAY_MENU_STATUS_NONE;
    return MENU_ACTION_RESULT_COMPLETED;
  }

  if (action == DISPLAY_MENU_ACTION_DELETE_WORLD) {
    if (server_session.running && server_session.world_slot == world_slot) {
      if (g_log != nullptr) {
        std::fprintf(g_log,
                     "live_menu_action_result action=%" PRIu32
                     " status=active_world_not_deleted world_slot=%" PRIu32
                     "\n",
                     action, world_slot);
        std::fflush(g_log);
      }
      menu_status_code = DISPLAY_MENU_STATUS_ACTIVE_WORLD;
      return MENU_ACTION_RESULT_FAILED;
    }
    const bool removed = remove_world_save(world_slot);
    menu_status_code = removed ? DISPLAY_MENU_STATUS_DELETED
                               : DISPLAY_MENU_STATUS_MISSING_WORLD;
    if (g_log != nullptr) {
      std::fprintf(g_log,
                   "live_menu_action_result action=%" PRIu32
                   " status=%s world_slot=%" PRIu32 "\n",
                   action, removed ? "deleted" : "missing_world", world_slot);
      std::fflush(g_log);
    }
    return removed ? MENU_ACTION_RESULT_COMPLETED : MENU_ACTION_RESULT_FAILED;
  }

  if (action == DISPLAY_MENU_ACTION_CONNECT_SERVER ||
      action == DISPLAY_MENU_ACTION_CONNECT_LOCAL) {
    const char *address =
        action == DISPLAY_MENU_ACTION_CONNECT_LOCAL ? "127.0.0.1"
                                                    : server_address;
    if (!valid_address_text(address) || !valid_port_text(server_port)) {
      menu_status_code = DISPLAY_MENU_STATUS_INVALID_SERVER;
      if (g_log != nullptr) {
        std::fprintf(g_log,
                     "live_menu_action_result action=%" PRIu32
                     " status=invalid_server_endpoint address=%s port=%s\n",
                     action, address != nullptr ? address : "",
                     server_port != nullptr ? server_port : "");
        std::fflush(g_log);
      }
      return MENU_ACTION_RESULT_FAILED;
    }
    if (is_local_address(address)) {
      if (!attach_local_file_bridge(server_session)) {
        menu_status_code = DISPLAY_MENU_STATUS_FAILED;
        result = -9;
        return MENU_ACTION_RESULT_FATAL;
      }
      menu_status_code = DISPLAY_MENU_STATUS_CONNECTED;
      if (g_log != nullptr) {
        std::fprintf(g_log,
                     "live_menu_action_result action=%" PRIu32
                     " status=connected_local_bridge address=%s port=%s "
                     "chunk_stream=%s\n",
                     action, address, server_port,
                     server_session.chunk_stream_path.string().c_str());
        std::fflush(g_log);
      }
      return MENU_ACTION_RESULT_COMPLETED;
    }
    if (g_log != nullptr) {
      std::fprintf(g_log,
                   "live_menu_action_result action=%" PRIu32
                   " status=network_transport_not_available address=%s port=%s\n",
                   action, address, server_port);
      std::fflush(g_log);
    }
    menu_status_code = DISPLAY_MENU_STATUS_UNAVAILABLE;
    return MENU_ACTION_RESULT_FAILED;
  }

  if (action == DISPLAY_MENU_ACTION_SAVE_WORLD) {
    if (server_session.world_root.empty()) {
      menu_status_code = DISPLAY_MENU_STATUS_MISSING_WORLD;
      if (g_log != nullptr) {
        std::fprintf(g_log,
                     "live_menu_action_result action=%" PRIu32
                     " status=no_active_world\n",
                     action);
        std::fflush(g_log);
      }
      return MENU_ACTION_RESULT_FAILED;
    }
    if (!write_world_metadata(server_session.world_root,
                              server_session.world_slot, world_name, true)) {
      menu_status_code = DISPLAY_MENU_STATUS_FAILED;
      result = -9;
      return MENU_ACTION_RESULT_FATAL;
    }
    menu_status_code = DISPLAY_MENU_STATUS_SAVED;
    if (g_log != nullptr) {
      std::fprintf(g_log,
                   "live_menu_action_result action=%" PRIu32
                   " status=saved world_slot=%" PRIu32 " world=%s\n",
                   action, server_session.world_slot,
                   server_session.world_root.string().c_str());
      std::fflush(g_log);
    }
    return MENU_ACTION_RESULT_COMPLETED;
  }

  if (action != DISPLAY_MENU_ACTION_CREATE_WORLD &&
      action != DISPLAY_MENU_ACTION_LOAD_WORLD) {
    if (g_log != nullptr && action != DISPLAY_MENU_ACTION_NONE) {
      std::fprintf(g_log,
                   "live_menu_action_result action=%" PRIu32
                   " status=deferred\n",
                   action);
      std::fflush(g_log);
    }
    menu_status_code = DISPLAY_MENU_STATUS_FAILED;
    return MENU_ACTION_RESULT_FAILED;
  }

  if (!server_session.running &&
      !prepare_singleplayer_server_session(server_session,
                                           game_modules_disabled,
                                           world_slot)) {
    menu_status_code = DISPLAY_MENU_STATUS_FAILED;
    result = -9;
    return MENU_ACTION_RESULT_FATAL;
  }
  if (action == DISPLAY_MENU_ACTION_CREATE_WORLD &&
      selected_world_exists(server_session.world_root)) {
    if (g_log != nullptr) {
      std::fprintf(g_log,
                   "live_menu_action_result action=%" PRIu32
                   " status=world_exists world_slot=%" PRIu32 " world=%s\n",
                   action, world_slot,
                   server_session.world_root.string().c_str());
      std::fflush(g_log);
    }
    menu_status_code = DISPLAY_MENU_STATUS_WORLD_EXISTS;
    return MENU_ACTION_RESULT_FAILED;
  }
  if (action == DISPLAY_MENU_ACTION_CREATE_WORLD &&
      !write_world_metadata(server_session.world_root, world_slot, world_name,
                            false)) {
    menu_status_code = DISPLAY_MENU_STATUS_FAILED;
    result = -9;
    return MENU_ACTION_RESULT_FATAL;
  }
  if (!selected_world_exists(server_session.world_root)) {
    menu_status_code = DISPLAY_MENU_STATUS_MISSING_WORLD;
    if (g_log != nullptr) {
      std::fprintf(g_log,
                   "live_menu_action_result action=%" PRIu32
                   " status=missing_world world=%s\n",
                   action, server_session.world_root.string().c_str());
      std::fflush(g_log);
    }
    return MENU_ACTION_RESULT_FAILED;
  }

  const chunk_view initial_chunk_view =
      chunk_view_for_camera(camera.position[0], camera.position[2],
                            render_distance);
  chunk_view empty_previous_view{
      std::numeric_limits<int>::min(),
      std::numeric_limits<int>::min(),
      0,
  };
  octaryn_host_frame_snapshot initial_frame =
      create_frame(0u, kStartupDeltaSeconds);
  client_input_debug_state initial_input{};
  apply_input_to_frame(initial_frame, initial_input, camera);
  if (!write_chunk_view_intent(initial_chunk_view, empty_previous_view, 1u) ||
      !write_player_input_intent(initial_frame) ||
      !write_world_time_intent(server_session, world_time_controls) ||
      !start_singleplayer_server(server_session)) {
    menu_status_code = DISPLAY_MENU_STATUS_FAILED;
    result = -9;
    return MENU_ACTION_RESULT_FATAL;
  }

  menu_status_code = action == DISPLAY_MENU_ACTION_CREATE_WORLD
                         ? DISPLAY_MENU_STATUS_CREATED
                         : DISPLAY_MENU_STATUS_LOADED;
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "live_menu_action_result action=%" PRIu32
                 " status=started world_slot=%" PRIu32 " world=%s\n",
                 action, world_slot,
                 server_session.world_blocks_path.string().c_str());
    std::fflush(g_log);
  }
  return MENU_ACTION_RESULT_COMPLETED;
}

} // namespace octaryn_client_app
