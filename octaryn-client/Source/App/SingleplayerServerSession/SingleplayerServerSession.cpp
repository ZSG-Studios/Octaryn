#include "SingleplayerServerSession.h"

#include "Environment.h"
#include "Log.h"

#include <SDL3/SDL.h>

#include <cinttypes>
#include <cstdlib>
#include <cstdio>
#include <string>

#if !defined(_WIN32)
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
#endif

namespace octaryn_client_app {

namespace {

constexpr const char *kDisableSingleplayerServerFlag =
    "OCTARYN_CLIENT_DISABLE_SINGLEPLAYER_SERVER";
constexpr const char *kSingleplayerSessionRootEnv =
    "OCTARYN_CLIENT_SINGLEPLAYER_SESSION_ROOT";
constexpr const char *kSingleplayerWorldRootEnv =
    "OCTARYN_CLIENT_SINGLEPLAYER_WORLD_ROOT";

} // namespace

bool prepare_singleplayer_server_session(singleplayer_server_session &session,
                                         bool game_modules_disabled,
                                         uint32_t world_slot) {
  if (read_enabled_flag(kDisableSingleplayerServerFlag) ||
      env_value_is_present("OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH")) {
    return true;
  }

  char entrypoint_buffer[4096] = {};
  if (!build_client_bundle_path(entrypoint_buffer, sizeof(entrypoint_buffer),
                                "server/Octaryn.Server",
                                "client_server_entrypoint_path=failed")) {
    return true;
  }

  const std::filesystem::path entrypoint(entrypoint_buffer);
  if (!std::filesystem::exists(entrypoint)) {
    log_line("client_server_supervisor active=0 reason=missing_entrypoint");
    return true;
  }

  std::error_code error;
  const std::filesystem::path workspace_root =
      std::filesystem::current_path(error);
  if (error) {
    log_line("client_server_supervisor active=0 reason=current_path_failed");
    return true;
  }

  const char *configured_root = std::getenv(kSingleplayerSessionRootEnv);
  if (configured_root != nullptr && configured_root[0] != '\0') {
    session.root = std::filesystem::path(configured_root);
  } else {
    const uint64_t session_id = SDL_GetTicksNS();
    session.root = workspace_root / "logs" / "server" /
                   ("singleplayer-" + std::to_string(session_id));
  }
  const char *configured_world_root = std::getenv(kSingleplayerWorldRootEnv);
  if (configured_world_root != nullptr && configured_world_root[0] != '\0') {
    session.world_root = std::filesystem::path(configured_world_root);
  } else {
    const uint32_t slot = world_slot < 3u ? world_slot + 1u : 1u;
    session.world_root = workspace_root / "saves" / "singleplayer" /
                         ("world" + std::to_string(slot));
  }
  session.world_slot = world_slot < 3u ? world_slot : 0u;
  session.entrypoint = entrypoint;
  session.world_blocks_path = session.world_root / "world_blocks.json";
  session.chunk_view_intent_path = session.root / "chunk_view_intent.json";
  session.chunk_stream_path = session.root / "chunk_stream.json";
  session.player_input_intent_path = session.root / "player_input_intent.json";
  session.block_interaction_intent_path =
      session.root / "block_interaction_intent.json";
  session.world_time_intent_path = session.root / "world_time_intent.json";
  session.server_log_path = session.root / "server_live.log";
  std::filesystem::create_directories(session.root, error);
  if (error) {
    log_line("client_server_supervisor active=0 reason=session_dir_failed");
    return true;
  }
  std::filesystem::create_directories(session.world_root, error);
  if (error) {
    log_line("client_server_supervisor active=0 reason=world_dir_failed");
    return true;
  }

  const bool env_ready =
      set_process_env("OCTARYN_CLIENT_APP_CHUNK_STREAM_PATH",
                      session.chunk_stream_path) &&
      set_process_env("OCTARYN_CLIENT_CHUNK_VIEW_INTENT_PATH",
                      session.chunk_view_intent_path) &&
      set_process_env("OCTARYN_CLIENT_PLAYER_INPUT_INTENT_PATH",
                      session.player_input_intent_path) &&
      set_process_env("OCTARYN_CLIENT_BLOCK_INTERACTION_INTENT_PATH",
                      session.block_interaction_intent_path) &&
      set_process_env("OCTARYN_SERVER_WORLD_BLOCKS_PATH",
                      session.world_blocks_path) &&
      set_process_env("OCTARYN_SERVER_CHUNK_VIEW_INTENT_PATH",
                      session.chunk_view_intent_path) &&
      set_process_env("OCTARYN_SERVER_CHUNK_STREAM_PATH",
                      session.chunk_stream_path) &&
      set_process_env("OCTARYN_SERVER_PLAYER_SAVE_ROOT", session.world_root) &&
      set_process_env("OCTARYN_SERVER_PLAYER_INPUT_INTENT_PATH",
                      session.player_input_intent_path) &&
      set_process_env("OCTARYN_SERVER_BLOCK_INTERACTION_INTENT_PATH",
                      session.block_interaction_intent_path) &&
      set_process_env("OCTARYN_SERVER_WORLD_TIME_INTENT_PATH",
                      session.world_time_intent_path) &&
      set_process_env("OCTARYN_SERVER_LIVE_DEBUG_LOG_PATH",
                      session.server_log_path) &&
      set_process_env_text("OCTARYN_SERVER_LIVE_DEBUG_FILTER_STEADY", "1") &&
      set_process_env_text("OCTARYN_SERVER_PROCESS_STREAM_LIVE", "1") &&
      set_process_env_text("OCTARYN_SERVER_CHUNK_STREAM_METADATA_ONLY", "1");

  if (game_modules_disabled) {
    set_process_env_text("OCTARYN_SERVER_DISABLE_GAME_MODULES", "1");
  }

  if (!env_ready) {
    log_line("client_server_supervisor active=0 reason=env_failed");
    return true;
  }

  session.enabled = true;
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "client_server_supervisor prepared=1 entrypoint=%s root=%s "
                 "world_root=%s game_modules_disabled=%d\n",
                 session.entrypoint.string().c_str(),
                 session.root.string().c_str(),
                 session.world_root.string().c_str(),
                 game_modules_disabled ? 1 : 0);
    std::fflush(g_log);
  }
  return true;
}

bool start_singleplayer_server(singleplayer_server_session &session) {
  if (!session.enabled || session.running) {
    return true;
  }

#if defined(_WIN32)
  log_line("client_server_supervisor active=0 reason=unsupported_platform");
  return true;
#else
  std::string entrypoint = session.entrypoint.string();
  std::string console_log = (session.root / "server_console.log").string();
  char *argv[] = {entrypoint.data(), nullptr};
  pid_t process_id = -1;
  posix_spawn_file_actions_t actions{};
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO,
                                   console_log.c_str(),
                                   O_WRONLY | O_CREAT | O_TRUNC, 0644);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO,
                                   console_log.c_str(),
                                   O_WRONLY | O_CREAT | O_APPEND, 0644);
  const int spawn_result =
      posix_spawn(&process_id, entrypoint.c_str(), &actions, nullptr, argv,
                  environ);
  posix_spawn_file_actions_destroy(&actions);
  if (spawn_result != 0) {
    if (g_log != nullptr) {
      std::fprintf(g_log,
                   "client_server_supervisor active=0 reason=spawn_failed "
                   "errno=%d\n",
                   spawn_result);
      std::fflush(g_log);
    }
    return false;
  }

  session.process_id = process_id;
  session.running = true;
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "client_server_supervisor active=1 pid=%d entrypoint=%s "
                 "chunk_stream=%s\n",
                 static_cast<int>(process_id), entrypoint.c_str(),
                 session.chunk_stream_path.string().c_str());
    std::fflush(g_log);
  }
  return true;
#endif
}

void stop_singleplayer_server(singleplayer_server_session &session) {
  if (!session.running) {
    return;
  }

#if !defined(_WIN32)
  kill(session.process_id, SIGTERM);
  int status = 0;
  waitpid(session.process_id, &status, 0);
  if (g_log != nullptr) {
    std::fprintf(g_log,
                 "client_server_supervisor stopped=1 pid=%d status=%d\n",
                 static_cast<int>(session.process_id), status);
    std::fflush(g_log);
  }
#endif
  session.running = false;
}

} // namespace octaryn_client_app
