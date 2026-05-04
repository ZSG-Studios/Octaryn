#pragma once

#include <filesystem>

#if !defined(_WIN32)
#include <sys/types.h>
#endif

namespace octaryn_client_app {

struct singleplayer_server_session {
  bool enabled = false;
  bool running = false;
#if !defined(_WIN32)
  pid_t process_id = -1;
#endif
  std::filesystem::path root;
  std::filesystem::path entrypoint;
  std::filesystem::path world_blocks_path;
  std::filesystem::path chunk_view_intent_path;
  std::filesystem::path chunk_stream_path;
  std::filesystem::path player_input_intent_path;
  std::filesystem::path block_interaction_intent_path;
  std::filesystem::path world_time_intent_path;
  std::filesystem::path server_log_path;
};

bool prepare_singleplayer_server_session(singleplayer_server_session &session,
                                         bool game_modules_disabled);
bool start_singleplayer_server(singleplayer_server_session &session);
void stop_singleplayer_server(singleplayer_server_session &session);

} // namespace octaryn_client_app
