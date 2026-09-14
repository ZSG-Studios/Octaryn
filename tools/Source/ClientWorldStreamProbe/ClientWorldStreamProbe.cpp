#include "StreamSnapshot.h"
#include "TerrainGeneration.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace octaryn::client::world_presentation;
namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template <typename T> void write(std::ofstream& out, T value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void snapshot_file(const std::filesystem::path& path, std::uint64_t seed = 1337,
                   std::uint32_t mode = 0, std::uint32_t revision = 2,
                   std::uint32_t schema = 2, std::uint16_t top_block = 5) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write("OCSTRM01", 8);
  write(out, schema); write(out, std::uint64_t{42});
  write(out, -1); write(out, -1); write(out, 2u); write(out, seed);
  if (schema == 2) { write(out, mode); write(out, revision); }
  write(out, std::uint64_t{}); write(out, 0u); write(out, 0.0); write(out, 0.0f);
  for (int i = 0; i < 8; ++i) write(out, 0.0f);
  write(out, 0u); write(out, 1u); write(out, 1u); write(out, 2u);
  write(out, -1); write(out, -1); write(out, -32); write(out, -32);
  write(out, 0u); write(out, 2u);
  write(out, -32); write(out, -256); write(out, -32); write(out, std::uint16_t{0});
  write(out, -1); write(out, 255); write(out, -1); write(out, top_block);
  require(static_cast<bool>(out), "write snapshot");
}
std::size_t index(int x, int y, int z) {
  return static_cast<std::size_t>(x + 32 * (y + 256 + 512 * z));
}
std::uint64_t voxel_hash(const StreamColumn& column) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const auto block : column.blocks) {
    hash ^= block;
    hash *= 1099511628211ull;
  }
  return hash;
}
std::uint64_t validate_generation() {
  const OctarynServerTerrainMaterialRules rules{30, 14, 3, 1, 2, 5, 4};
  constexpr std::array<std::pair<int, int>, 10> coordinates{{
      {0, 0}, {-1, -1}, {1, 0}, {0, 1}, {-1, 0}, {4, -3},
      {-31, 27}, {99, 105}, {-1024, 1024}, {-1000000, 1000000}}};
  std::array<std::uint64_t, coordinates.size()> hashes{};
  std::uint64_t checked = 0;
  std::size_t retained_bytes = 0, maximum_retained_bytes = 0;
  double elapsed_ms = 0.0, maximum_ms = 0.0;
  const auto generate = [&](int cx, int cz) {
    const auto start = std::chrono::steady_clock::now();
    auto column = generate_stream_column({cx, cz, 7, {}}, 42);
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    elapsed_ms += ms;
    maximum_ms = std::max(maximum_ms, ms);
    return column;
  };
  for (std::size_t i = 0; i < coordinates.size(); ++i) {
    const auto [cx, cz] = coordinates[i];
    const auto column = generate(cx, cz);
    require(column.blocks.size() == 32 * 512 * 32, "full depth voxel count");
    require(column.blocks.is_compact(), "generated columns must publish compact storage");
    const auto bytes = column.blocks.storage_bytes();
    require(bytes < column.blocks.size(), "representative terrain must use less than half dense uint16 storage");
    retained_bytes += bytes; maximum_retained_bytes = std::max(maximum_retained_bytes, bytes);
    auto shared_column = column;
    require(shared_column.blocks.storage_identity() == column.blocks.storage_identity(),
        "query/renderer column copy must share generated payload");
    require(column.x == cx && column.z == cz && column.epoch == 42 && column.revision == 7,
        "generated column identity");
    hashes[i] = voxel_hash(column);
    const auto compare = [&](int x, int y, int z) {
      require(y >= -256 && y < 256, "sample in client volume");
      std::uint16_t expected{};
      require(octaryn_server_terrain_generated_block(cx * 32 + x, y, cz * 32 + z, &rules, &expected) == 0,
          "server generated block");
      if (column.blocks[index(x, y, z)] != expected) {
        std::cerr << "parity mismatch world=" << cx * 32 + x << ',' << y << ',' << cz * 32 + z << '\n';
        require(false, "authority/client reconstruction parity");
      }
      ++checked;
    };
    for (const auto z : {0, 1, 15, 30, 31}) for (const auto x : {0, 1, 15, 30, 31}) {
      OctarynServerTerrainColumnPlan plan{};
      require(octaryn_server_terrain_plan_column(cx * 32 + x, cz * 32 + z, &rules, &plan) == 0, "server column plan");
      for (const auto y : {-256, -253, -252, -192, -64, -1, 0, 29, 30,
               plan.terrain_height - 9, plan.terrain_height - 8, plan.terrain_height - 4,
               plan.terrain_height - 1, plan.terrain_height, plan.terrain_height + 1, 255}) {
        compare(x, y, z);
      }
    }
    // Full vertical seams include both sides of positive and negative chunk borders.
    for (const auto x : {0, 31}) for (const auto z : {0, 31})
      for (int y = -256; y < 256; ++y) compare(x, y, z);
  }
  std::array<std::size_t, coordinates.size()> order{};
  std::iota(order.begin(), order.end(), std::size_t{});
  std::mt19937 random(5719);
  std::shuffle(order.begin(), order.end(), random);
  for (const auto i : order) {
    const auto [cx, cz] = coordinates[i];
    require(voxel_hash(generate(cx, cz)) == hashes[i], "shuffled whole-column deterministic generation");
  }
  std::cout << "terrain_column_generation columns=" << coordinates.size() * 2
      << " mean_ms=" << elapsed_ms / static_cast<double>(coordinates.size() * 2)
      << " max_ms=" << maximum_ms << " order=passed\n";
  std::cout << "terrain_column_storage columns=" << coordinates.size()
      << " mean_bytes=" << retained_bytes / coordinates.size() << " max_bytes=" << maximum_retained_bytes
      << " dense_bytes=1048576 sharing=passed\n";
  return checked;
}
void validate_delivery_queries(const std::filesystem::path& path) {
  WorldStream stream(path);
  stream.request(-1, -1, 2);
  // Withhold delivery while the real worker parses/generates. Camera/target
  // queries must stay on the last delivered data, including edited air.
  const auto hold = [&](bool available, std::uint16_t expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    do {
      std::uint16_t block{};
      const bool found = stream.try_block(-1, 255, -1, block);
      require(found == available && (!found || block == expected),
          "query changed before corresponding renderer delivery");
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (std::chrono::steady_clock::now() < deadline);
  };
  const auto deliver = [&](std::uint16_t expected) {
    StreamColumn result;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool received = false;
    while (std::chrono::steady_clock::now() < deadline && !(received = stream.poll(result)))
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    require(received && result.x == -1 && result.z == -1 &&
        result.blocks[index(0, -256, 0)] == 0 && result.blocks[index(31, 255, 31)] == expected,
        "async authoritative delivery");
    std::uint16_t block{};
    require(stream.try_block(-1, 255, -1, block) && block == expected,
        "query must match delivered column immediately");
  };
  hold(false, 0);
  deliver(5);
  snapshot_file(path, 1337, 0, 2, 2, 0);
  hold(true, 5);
  deliver(0);
  snapshot_file(path);
  hold(true, 0);
  deliver(5);
  // No worker maintenance is required between these two frame requests.
  stream.request(100, 100, 2);
  std::uint16_t block{};
  require(!stream.try_block(-1, 255, -1, block), "retired column query must be unavailable");
  stream.request(-1, -1, 2);
  hold(false, 0);
  deliver(5);
}
}

int main() {
  const auto path = std::filesystem::temp_directory_path() /
      ("octaryn-world-stream-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
  try {
    const auto checked = validate_generation();
    snapshot_file(path);
    StreamSnapshot snapshot;
    std::string error;
    require(read_stream_snapshot(path, snapshot, error), "read authoritative binary");
#if defined(_WIN32)
    // A publisher may already hold delete access while a consumer opens its reader.
    const auto publisher = CreateFileW(path.c_str(), DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    require(publisher != INVALID_HANDLE_VALUE, "open replacement publisher");
    const bool shared_read = read_stream_snapshot(path, snapshot, error);
    CloseHandle(publisher);
    require(shared_read, "snapshot reader shares publisher delete access");
#endif
    require(snapshot.epoch == 42 && snapshot.columns.size() == 1, "snapshot metadata");
    const auto edited = generate_stream_column(snapshot.columns.front(), snapshot.epoch);
    require(edited.blocks[index(0, -256, 0)] == 0 && edited.blocks[index(31, 255, 31)] == 5, "air and top boundary overrides");
    const auto revision = snapshot.columns.front().revision;
    snapshot_file(path, 99);
    require(!read_stream_snapshot(path, snapshot, error) && snapshot.columns.front().revision == revision, "reject unsupported seed without changing valid snapshot");
    for (const auto mode : {1u, 2u, 3u}) {
      snapshot_file(path, 1337, mode);
      require(!read_stream_snapshot(path, snapshot, error) && snapshot.epoch == 42 && snapshot.columns.front().revision == revision,
          "reject unsupported generator mode atomically");
    }
    snapshot_file(path, 1337, 0, 1);
    require(!read_stream_snapshot(path, snapshot, error) && snapshot.epoch == 42, "reject generator revision mismatch atomically");
    snapshot_file(path, 1337, 0, 2, 1);
    require(!read_stream_snapshot(path, snapshot, error) && snapshot.epoch == 42, "reject unversioned terrain identity atomically");
    snapshot_file(path);
    std::filesystem::resize_file(path, 115);
    require(!read_stream_snapshot(path, snapshot, error) && snapshot.epoch == 42, "reject truncated snapshot atomically");
    snapshot_file(path);
    validate_delivery_queries(path);
    std::filesystem::remove(path);
    std::cout << "client_world_stream_probe=passed parity_samples=" << checked << " parser=passed edits=passed async=passed query_delivery=passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::cerr << "client_world_stream_probe=failed " << error.what() << '\n';
    return 1;
  }
}
