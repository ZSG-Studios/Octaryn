#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace octaryn::tools::server_world_persistence_probe {

bool validate_chunk_override_directory_scan() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "octaryn_server_chunk_override_directory_scan_probe";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);

  const octaryn_server_persistence_chunk_override_block blocks[]{
      {.bx = 0, .by = 2, .bz = 0, .block = 5u},
      {.bx = 33, .by = 3, .bz = 0, .block = 6u}};
  const octaryn_server_persistence_chunk_override_file first_file{
      .version = 2u, .cx = 0, .cz = 0, .block_count = 1u};
  const octaryn_server_persistence_chunk_override_file second_file{
      .version = 2u, .cx = 32, .cz = 0, .block_count = 1u};
  const octaryn_server_persistence_chunk_override_file mismatched_file{
      .version = 2u, .cx = 64, .cz = 0, .block_count = 1u};

  bool ok = true;
  const std::string first_path = (root / "chunk_0_0.json").string();
  const std::string second_path = (root / "chunk_32_0.json").string();
  const std::string mismatch_path = (root / "chunk_96_0.json").string();
  ok &= expect_equal("scan write first",
                     octaryn_server_persistence_write_chunk_override_file(
                         first_path.c_str(), &first_file, blocks),
                     0);
  ok &= expect_equal("scan write second",
                     octaryn_server_persistence_write_chunk_override_file(
                         second_path.c_str(), &second_file, blocks + 1),
                     0);
  ok &= expect_equal("scan write mismatched",
                     octaryn_server_persistence_write_chunk_override_file(
                         mismatch_path.c_str(), &mismatched_file, blocks),
                     0);
  {
    std::ofstream ignored(root / "chunk_bad_0.json",
                          std::ios::binary | std::ios::trunc);
    ignored << R"({"version":2,"cx":0,"cz":0,"blocks":[]})";
  }

  octaryn_server_persistence_chunk_override_directory_scan scan{};
  ok &= expect_equal("scan current without aggregate",
                     octaryn_server_persistence_scan_chunk_override_directory(
                         root.string().c_str(), "", &scan),
                     0);
  ok &= expect_equal("scan current flag",
                     scan.current_files_at_least_as_new_as, 1u);
  ok &= expect_equal("scan file count", scan.file_count, 2u);
  ok &= expect_equal("scan block count", scan.block_count, 2u);

  std::filesystem::remove_all(root, error);
  return ok;
}

bool validate_chunk_override_directory_prune() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "octaryn_server_chunk_override_directory_prune_probe";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);

  const octaryn_server_persistence_chunk_override_block block{
      .bx = 0, .by = 2, .bz = 0, .block = 5u};
  const octaryn_server_persistence_chunk_override_file kept_file{
      .version = 2u, .cx = 0, .cz = 0, .block_count = 1u};
  const octaryn_server_persistence_chunk_override_file kept_negative_file{
      .version = 2u, .cx = -32, .cz = -64, .block_count = 1u};
  const octaryn_server_persistence_chunk_override_file stale_file{
      .version = 2u, .cx = 32, .cz = 0, .block_count = 1u};
  const octaryn_server_persistence_chunk_override_file stale_negative_file{
      .version = 2u, .cx = -96, .cz = 32, .block_count = 1u};

  const std::string kept_path = (root / "chunk_0_0.json").string();
  const std::string kept_negative_path =
      (root / "chunk_-32_-64.json").string();
  const std::string stale_path = (root / "chunk_32_0.json").string();
  const std::string stale_negative_path =
      (root / "chunk_-96_32.json").string();
  const std::string ignored_path = (root / "chunk_bad_0.json").string();

  bool ok = true;
  ok &= expect_equal("prune write kept",
                     octaryn_server_persistence_write_chunk_override_file(
                         kept_path.c_str(), &kept_file, &block),
                     0);
  ok &= expect_equal("prune write kept negative",
                     octaryn_server_persistence_write_chunk_override_file(
                         kept_negative_path.c_str(), &kept_negative_file,
                         &block),
                     0);
  ok &= expect_equal("prune write stale",
                     octaryn_server_persistence_write_chunk_override_file(
                         stale_path.c_str(), &stale_file, &block),
                     0);
  ok &= expect_equal("prune write stale negative",
                     octaryn_server_persistence_write_chunk_override_file(
                         stale_negative_path.c_str(), &stale_negative_file,
                         &block),
                     0);
  {
    std::ofstream ignored(ignored_path, std::ios::binary | std::ios::trunc);
    ignored << R"({"version":2,"cx":0,"cz":0,"blocks":[]})";
  }

  const octaryn_server_persistence_chunk_column planned_columns[]{
      {.origin_x = 0, .origin_z = 0, .block_offset = 0u, .block_count = 1u},
      {.origin_x = -32,
       .origin_z = -64,
       .block_offset = 1u,
       .block_count = 1u}};
  uint32_t removed = 0u;
  ok &= expect_equal("prune stale result",
                     octaryn_server_persistence_prune_stale_chunk_override_files(
                         root.string().c_str(), planned_columns, 2u, &removed),
                     0);
  ok &= expect_equal("prune stale removed", removed, 2u);
  ok &= expect_equal("prune kept exists", std::filesystem::exists(kept_path),
                     true);
  ok &= expect_equal("prune kept negative exists",
                     std::filesystem::exists(kept_negative_path), true);
  ok &= expect_equal("prune stale removed file",
                     std::filesystem::exists(stale_path), false);
  ok &= expect_equal("prune stale negative removed file",
                     std::filesystem::exists(stale_negative_path), false);
  ok &= expect_equal("prune ignored malformed name",
                     std::filesystem::exists(ignored_path), true);

  std::filesystem::remove_all(root, error);
  return ok;
}

} // namespace octaryn::tools::server_world_persistence_probe
