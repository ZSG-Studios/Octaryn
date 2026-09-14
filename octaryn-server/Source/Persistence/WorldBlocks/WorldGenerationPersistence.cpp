#include "WorldPersistence.h"
#include "PersistenceFileIO.h"

#include <glaze/glaze.hpp>

#include <filesystem>
#include <string>

namespace octaryn::server::persistence {
struct world_generation_file {
  uint32_t version{};
  std::string generator;
  uint32_t revision{};
  uint64_t seed{};
  uint32_t mode{};
};
} // namespace octaryn::server::persistence

namespace {
using octaryn::server::persistence::world_generation_file;
constexpr glz::opts ReadOptions{.error_on_unknown_keys = true};
constexpr glz::opts WriteOptions{.prettify = true};

bool has_saved_files(const std::filesystem::path& directory) {
  if (!std::filesystem::exists(directory)) return false;
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    const auto name = entry.path().filename().string();
    if (name == "world_time.json" || name == "world_meta.json" ||
        name == "world_blocks.json" || name == "chunks" ||
        (name.starts_with("player_") && name.ends_with(".json")) ||
        (name.starts_with("chunk_") && name.ends_with(".json"))) return true;
  }
  return false;
}

bool matches(const world_generation_file& saved, const world_generation_file& expected) {
  return saved.version == expected.version && saved.generator == expected.generator &&
      saved.revision == expected.revision && saved.seed == expected.seed && saved.mode == expected.mode;
}
} // namespace

extern "C" int32_t octaryn_server_persistence_ensure_world_generation(
    const char* world_root, const char* aggregate_path, const char* player_root,
    uint32_t mode) {
  return octaryn_server_persistence_ensure_world_generation_revision(
      world_root, aggregate_path, player_root, mode, 0);
}

extern "C" int32_t octaryn_server_persistence_ensure_world_generation_revision(
    const char* world_root, const char* aggregate_path, const char* player_root,
    uint32_t mode, uint32_t revision) {
  if (!world_root || !*world_root || !aggregate_path || !*aggregate_path ||
      !player_root || !*player_root || mode > 2u) return -1;
  if (revision != 0 && (mode == 0 ? revision != 2 && revision != 3 : revision != 1)) return -1;
  try {
    const auto root = std::filesystem::path(world_root);
    const auto path = root / "world_generation.json";
    world_generation_file expected{1u, "octaryn.basegame",
        revision != 0 ? revision : (mode == 0u ? 3u : 1u), 1337u, mode};
    if (std::filesystem::exists(path)) {
      std::string payload;
      world_generation_file saved{};
      if (!octaryn::server::persistence::read_text_file(path, payload) ||
          glz::read<ReadOptions>(saved, payload)) return -2;
      if (revision == 0 && mode == 0 && saved.revision == 2) expected.revision = 2;
      return matches(saved, expected) ? 0 : -3;
    }
    if (std::filesystem::exists(aggregate_path) || has_saved_files(root) ||
        has_saved_files(player_root)) return -4;
    std::string payload;
    if (glz::write<WriteOptions>(expected, payload)) return -2;
    std::filesystem::create_directories(root);
    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::noreplace);
    if (!output) return -2;
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.flush();
    return output ? 0 : -2;
  } catch (const std::filesystem::filesystem_error&) {
    return -2;
  }
}

extern "C" int32_t octaryn_server_persistence_world_generation_revision(
    const char* world_root, uint32_t* revision) {
  if (!world_root || !*world_root || !revision) return -1;
  try {
    std::string payload;
    world_generation_file saved{};
    if (!octaryn::server::persistence::read_text_file(
            std::filesystem::path(world_root) / "world_generation.json", payload) ||
        glz::read<ReadOptions>(saved, payload)) return -2;
    if (saved.version != 1 || saved.generator != "octaryn.basegame" || saved.seed != 1337 ||
        saved.mode > 2 || (saved.mode == 0 ? saved.revision != 2 && saved.revision != 3 : saved.revision != 1)) return -3;
    *revision = saved.revision;
    return 0;
  } catch (const std::filesystem::filesystem_error&) { return -2; }
}
