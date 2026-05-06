#include "WorldPersistence.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view PlayerPrefix = "player_";
constexpr std::string_view PlayerExtension = ".json";

bool try_parse_player_id(const std::filesystem::path &path,
                         int32_t &player_id) {
  const std::string filename = path.filename().string();
  if (filename.size() <= PlayerPrefix.size() + PlayerExtension.size() ||
      !filename.starts_with(PlayerPrefix) ||
      !filename.ends_with(PlayerExtension)) {
    return false;
  }

  const std::string_view id_text(filename.data() + PlayerPrefix.size(),
                                 filename.size() - PlayerPrefix.size() -
                                     PlayerExtension.size());
  const char *begin = id_text.data();
  const char *end = begin + id_text.size();
  const auto result = std::from_chars(begin, end, player_id);
  return result.ec == std::errc{} && result.ptr == end;
}

std::vector<octaryn_server_persistence_player_file_entry>
read_player_entries(const char *directory) {
  std::vector<octaryn_server_persistence_player_file_entry> players;
  const std::filesystem::path root(directory);
  std::error_code error;
  if (!std::filesystem::is_directory(root, error) || error) {
    return players;
  }

  for (const std::filesystem::directory_entry &entry :
       std::filesystem::directory_iterator(root, error)) {
    if (error) {
      break;
    }

    std::error_code entry_error;
    if (!entry.is_regular_file(entry_error) || entry_error) {
      continue;
    }

    int32_t player_id = 0;
    if (!try_parse_player_id(entry.path(), player_id)) {
      continue;
    }

    octaryn_server_persistence_player_state state{};
    if (octaryn_server_persistence_read_player_file(
            entry.path().string().c_str(), &state) != 0) {
      continue;
    }

    players.push_back(octaryn_server_persistence_player_file_entry{
        .player_id = player_id,
        .state = state,
    });
  }

  std::ranges::sort(players, [](const auto &left, const auto &right) {
    return left.player_id < right.player_id;
  });
  return players;
}

std::filesystem::path player_path(const char *directory, int32_t player_id) {
  return std::filesystem::path(directory) /
         ("player_" + std::to_string(player_id) + ".json");
}

} // namespace

extern "C" {

int32_t octaryn_server_persistence_read_player_directory_count(
    const char *directory, uint32_t *player_count) {
  if (directory == nullptr || directory[0] == '\0' || player_count == nullptr) {
    return -1;
  }

  *player_count = static_cast<uint32_t>(read_player_entries(directory).size());
  return 0;
}

int32_t octaryn_server_persistence_read_player_directory_fill(
    const char *directory,
    octaryn_server_persistence_player_file_entry *players,
    uint32_t player_capacity, uint32_t *written) {
  if (directory == nullptr || directory[0] == '\0' || written == nullptr ||
      (players == nullptr && player_capacity != 0u)) {
    return -1;
  }

  const std::vector<octaryn_server_persistence_player_file_entry> entries =
      read_player_entries(directory);
  if (entries.size() > player_capacity) {
    return -2;
  }
  if (!entries.empty() && players == nullptr) {
    return -1;
  }

  std::ranges::copy(entries, players);
  *written = static_cast<uint32_t>(entries.size());
  return 0;
}

int32_t octaryn_server_persistence_read_player_directory_entry(
    const char *directory, int32_t player_id,
    octaryn_server_persistence_player_state *state) {
  if (directory == nullptr || directory[0] == '\0' || state == nullptr) {
    return -1;
  }

  return octaryn_server_persistence_read_player_file(
      player_path(directory, player_id).string().c_str(), state);
}

int32_t octaryn_server_persistence_write_player_directory_entry(
    const char *directory, int32_t player_id,
    const octaryn_server_persistence_player_state *state) {
  if (directory == nullptr || directory[0] == '\0' || state == nullptr) {
    return -1;
  }

  return octaryn_server_persistence_write_player_file(
      player_path(directory, player_id).string().c_str(), state);
}

int32_t octaryn_server_persistence_player_directory_path(
    const char *directory, int32_t player_id, char *path,
    uint64_t path_capacity, uint64_t *required_size) {
  if (directory == nullptr || directory[0] == '\0' ||
      required_size == nullptr) {
    return -1;
  }

  const std::string text = player_path(directory, player_id).string();
  const uint64_t required = static_cast<uint64_t>(text.size()) + 1u;
  *required_size = required;
  if (path == nullptr || path_capacity == 0u) {
    return 0;
  }

  if (path_capacity < required) {
    return -2;
  }

  std::memcpy(path, text.c_str(), static_cast<std::size_t>(required));
  return 0;
}

}
