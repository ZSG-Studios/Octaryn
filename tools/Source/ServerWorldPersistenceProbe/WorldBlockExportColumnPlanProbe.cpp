#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <cstdint>
#include <filesystem>
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

} // namespace

bool validate_world_block_export_column_plan() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "octaryn_server_world_block_export_column_probe";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  if (error) {
    return false;
  }

  const std::filesystem::path aggregate_path = root / "world_blocks.json";
  const std::vector<octaryn_server_persistence_block_edit> aggregate_edits{
      edit(-1, 2, 31, 6),
      edit(32, 3, 0, 7),
  };
  const octaryn_server_persistence_world_block_override_file aggregate_file{
      .version = 1u,
      .block_count = static_cast<uint32_t>(aggregate_edits.size()),
  };

  bool ok = true;
  ok &= expect_equal(
      "export aggregate write",
      octaryn_server_persistence_write_world_block_override_file(
          aggregate_path.string().c_str(), &aggregate_file,
          aggregate_edits.data()),
      0);

  octaryn_server_persistence_plan_counts counts{};
  ok &= expect_equal(
      "export aggregate count",
      octaryn_server_persistence_plan_world_block_export_columns_count(
          aggregate_path.string().c_str(), root.string().c_str(), &counts),
      0);
  ok &= expect_equal("export aggregate columns", counts.column_count, 2u);
  ok &= expect_equal("export aggregate blocks", counts.block_count, 2u);

  std::vector<octaryn_server_persistence_chunk_column> columns(
      counts.column_count);
  std::vector<octaryn_server_persistence_block_edit> ordered(
      counts.block_count);
  octaryn_server_persistence_plan_counts written{};
  ok &= expect_equal(
      "export aggregate fill",
      octaryn_server_persistence_plan_world_block_export_columns_fill(
          aggregate_path.string().c_str(), root.string().c_str(),
          columns.data(), static_cast<uint32_t>(columns.size()),
          ordered.data(), static_cast<uint32_t>(ordered.size()), &written),
      0);
  ok &= expect_equal("export aggregate first column x", columns[0].origin_x,
                     -32);
  ok &= expect_equal("export aggregate second column x", columns[1].origin_x,
                     32);
  ok &= expect_equal("export aggregate first block", ordered[0].block,
                     uint16_t{6});
  ok &= expect_equal("export aggregate second block", ordered[1].block,
                     uint16_t{7});

  uint32_t active_count = 0u;
  ok &= expect_equal(
      "active aggregate count",
      octaryn_server_persistence_read_world_block_overrides_count(
          aggregate_path.string().c_str(), root.string().c_str(),
          &active_count),
      0);
  ok &= expect_equal("active aggregate blocks", active_count, 2u);
  std::vector<octaryn_server_persistence_block_edit> active_edits(
      active_count);
  uint32_t active_written = 0u;
  ok &= expect_equal(
      "active aggregate fill",
      octaryn_server_persistence_read_world_block_overrides_fill(
          aggregate_path.string().c_str(), root.string().c_str(),
          active_edits.data(), active_count, &active_written),
      0);
  ok &= expect_equal("active aggregate written", active_written, 2u);
  ok &= expect_equal("active aggregate first block", active_edits[0].block,
                     uint16_t{6});

  const std::filesystem::path chunk_root = root / "chunk-only";
  const std::vector<octaryn_server_persistence_block_edit> chunk_edits{
      edit(64, 5, -1, 12),
  };
  ok &= expect_equal(
      "chunk-only save",
      octaryn_server_persistence_save_world_block_overrides(
          (chunk_root / "world_blocks.json").string().c_str(),
          chunk_root.string().c_str(), chunk_edits.data(),
          static_cast<uint32_t>(chunk_edits.size())),
      0);
  std::filesystem::remove(chunk_root / "world_blocks.json", error);
  ok &= expect_equal(
      "export chunk-only count",
      octaryn_server_persistence_plan_world_block_export_columns_count(
          (chunk_root / "world_blocks.json").string().c_str(),
          chunk_root.string().c_str(), &counts),
      0);
  ok &= expect_equal("export chunk-only columns", counts.column_count, 1u);
  ok &= expect_equal("export chunk-only blocks", counts.block_count, 1u);

  columns.assign(counts.column_count, {});
  ordered.assign(counts.block_count, {});
  ok &= expect_equal(
      "export chunk-only fill",
      octaryn_server_persistence_plan_world_block_export_columns_fill(
          (chunk_root / "world_blocks.json").string().c_str(),
          chunk_root.string().c_str(), columns.data(),
          static_cast<uint32_t>(columns.size()), ordered.data(),
          static_cast<uint32_t>(ordered.size()), &written),
      0);
  ok &= expect_equal("export chunk-only origin x", columns[0].origin_x, 64);
  ok &= expect_equal("export chunk-only origin z", columns[0].origin_z, -32);
  ok &= expect_equal("export chunk-only block", ordered[0].block,
                     uint16_t{12});
  ok &= expect_equal(
      "active chunk-only count",
      octaryn_server_persistence_read_world_block_overrides_count(
          (chunk_root / "world_blocks.json").string().c_str(),
          chunk_root.string().c_str(), &active_count),
      0);
  ok &= expect_equal("active chunk-only blocks", active_count, 1u);
  active_edits.assign(active_count, {});
  ok &= expect_equal(
      "active chunk-only fill",
      octaryn_server_persistence_read_world_block_overrides_fill(
          (chunk_root / "world_blocks.json").string().c_str(),
          chunk_root.string().c_str(), active_edits.data(), active_count,
          &active_written),
      0);
  ok &= expect_equal("active chunk-only written", active_written, 1u);
  ok &= expect_equal("active chunk-only block", active_edits[0].block,
                     uint16_t{12});

  std::filesystem::remove_all(root, error);
  return ok;
}

} // namespace octaryn::tools::server_world_persistence_probe
