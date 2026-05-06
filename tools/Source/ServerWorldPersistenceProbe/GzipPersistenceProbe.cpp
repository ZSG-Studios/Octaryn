#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace octaryn::tools::server_world_persistence_probe {

bool validate_gzip_round_trip() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "octaryn_server_world_persistence_probe_gzip";
  const std::filesystem::path path =
      root / "nested" / "octaryn_server_world_persistence_probe.gzip";
  const std::filesystem::path temp_path = path.string() + ".tmp";
  const std::string payload = "octaryn native gzip persistence";
  const std::string overwrite_payload = "octaryn native gzip overwrite";
  std::error_code error;
  std::filesystem::remove_all(root, error);

  bool ok = true;
  ok &= expect_equal("gzip write",
                     octaryn_server_persistence_write_gzip_file(
                         path.string().c_str(),
                         reinterpret_cast<const uint8_t *>(payload.data()),
                         payload.size()),
                     0);
  ok &= expect_equal("gzip temp replaced", std::filesystem::exists(temp_path),
                     false);
  ok &= expect_equal("gzip overwrite",
                     octaryn_server_persistence_write_gzip_file(
                         path.string().c_str(),
                         reinterpret_cast<const uint8_t *>(
                             overwrite_payload.data()),
                         overwrite_payload.size()),
                     0);

  uint64_t payload_size = 0u;
  ok &= expect_equal("gzip count",
                     octaryn_server_persistence_read_gzip_file_count(
                         path.string().c_str(), &payload_size),
                     0);
  ok &= expect_equal("gzip payload size", payload_size,
                     static_cast<uint64_t>(overwrite_payload.size()));

  std::vector<uint8_t> read_payload(payload_size);
  uint64_t written = 0u;
  ok &= expect_equal("gzip fill",
                     octaryn_server_persistence_read_gzip_file_fill(
                         path.string().c_str(), read_payload.data(),
                         read_payload.size(), &written),
                     0);
  ok &= expect_equal("gzip written size", written,
                     static_cast<uint64_t>(overwrite_payload.size()));
  ok &= expect_equal(
      "gzip payload",
      std::string_view(reinterpret_cast<const char *>(read_payload.data()),
                       read_payload.size()),
      std::string_view(overwrite_payload));

  std::filesystem::remove_all(root, error);
  return ok;
}

} // namespace octaryn::tools::server_world_persistence_probe
