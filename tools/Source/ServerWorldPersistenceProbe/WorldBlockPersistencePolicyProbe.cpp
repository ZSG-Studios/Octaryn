#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace octaryn::tools::server_world_persistence_probe {

namespace {

octaryn_server_persistence_block_edit edit(int32_t x, int32_t y, int32_t z,
                                           uint16_t block) {
  return octaryn_server_persistence_block_edit{
      .position = {.x = x, .y = y, .z = z},
      .block = block,
  };
}

bool select_source(const std::filesystem::path &directory,
                   const std::filesystem::path &aggregate,
                   uint32_t expected_source,
                   uint32_t expected_block_count) {
  octaryn_server_persistence_world_block_load_source source{};
  bool ok = true;
  ok &= expect_equal("select world-block load source",
                     octaryn_server_persistence_select_world_block_load_source(
                         directory.string().c_str(), aggregate.string().c_str(),
                         &source),
                     0);
  ok &= expect_equal("world-block load source", source.source,
                     expected_source);
  ok &= expect_equal("world-block source block count", source.block_count,
                     expected_block_count);
  return ok;
}

} // namespace

bool validate_world_block_persistence_policy() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "octaryn_server_world_block_persistence_policy_probe";
  const std::filesystem::path aggregate = root / "world_blocks.json";

  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  if (error) {
    return false;
  }

  bool ok = true;
  const std::vector<octaryn_server_persistence_block_edit> first_edits{
      edit(0, 1, 2, 5),
  };
  ok &= expect_equal(
      "world-block initialize",
      octaryn_server_persistence_initialize_world_block_overrides(
          aggregate.string().c_str(), root.string().c_str(), first_edits.data(),
          static_cast<uint32_t>(first_edits.size())),
      0);
  ok &= select_source(root, aggregate, 2u, 1u);

  const std::vector<octaryn_server_persistence_block_edit> newer_aggregate{
      edit(0, 1, 2, 99),
  };
  const octaryn_server_persistence_world_block_override_file aggregate_file{
      .version = 1u,
      .block_count = static_cast<uint32_t>(newer_aggregate.size()),
  };
  ok &= expect_equal("write newer aggregate",
                     octaryn_server_persistence_write_world_block_override_file(
                         aggregate.string().c_str(), &aggregate_file,
                         newer_aggregate.data()),
                     0);
  const std::filesystem::path sidecar = root / "chunk_0_0.json";
  const auto aggregate_time = std::filesystem::last_write_time(aggregate);
  std::filesystem::last_write_time(sidecar,
                                   aggregate_time - std::chrono::seconds(10),
                                   error);
  ok &= expect_equal("stale sidecar timestamp update", error ? 1 : 0, 0);
  ok &= select_source(root, aggregate, 1u, 1u);

  const std::vector<octaryn_server_persistence_block_edit> ignored_edits{
      edit(32, 3, 4, 7),
  };
  ok &= expect_equal(
      "world-block initialize existing aggregate",
      octaryn_server_persistence_initialize_world_block_overrides(
          aggregate.string().c_str(), root.string().c_str(),
          ignored_edits.data(), static_cast<uint32_t>(ignored_edits.size())),
      0);
  ok &= select_source(root, aggregate, 1u, 1u);

  ok &= expect_equal(
      "world-block save empty",
      octaryn_server_persistence_save_world_block_overrides(
          aggregate.string().c_str(), root.string().c_str(), nullptr, 0u),
      0);
  ok &= expect_equal("aggregate removed",
                     std::filesystem::exists(aggregate) ? 1 : 0, 0);
  ok &= select_source(root, aggregate, 0u, 0u);

  std::filesystem::remove_all(root, error);
  return ok;
}

} // namespace octaryn::tools::server_world_persistence_probe
