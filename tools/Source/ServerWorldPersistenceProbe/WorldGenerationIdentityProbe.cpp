#include "ProbeAssertions.h"
#include "WorldPersistence.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace octaryn::tools::server_world_persistence_probe {
namespace {
void write(const std::filesystem::path& path, const std::string& text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << text;
}
std::string read(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int ensure(const std::filesystem::path& root, uint32_t mode = 0u,
           const std::filesystem::path& player_root = {}) {
  return octaryn_server_persistence_ensure_world_generation(root.string().c_str(),
      (root / "world_blocks.json").string().c_str(),
      (player_root.empty() ? root : player_root).string().c_str(), mode);
}
} // namespace

bool validate_world_generation_identity() {
  const auto root = std::filesystem::temp_directory_path() /
      ("octaryn-generation-identity-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  bool ok = true;
  const auto fresh = root / "fresh";
  write(fresh / "stream.json.bin", "transient stream is not a save");
  ok &= expect_equal("create fresh identity", ensure(fresh), 0);
  const auto identity = fresh / "world_generation.json";
  const auto payload = read(identity);
  ok &= expect_equal("revision two recorded", payload.find("\"revision\": 2") != std::string::npos, true);
  write(fresh / "chunk_0_0.json", "authoritative air override");
  ok &= expect_equal("reload matching identity", ensure(fresh), 0);
  ok &= expect_equal("matching identity never rewritten", read(identity), payload);
  ok &= expect_equal("reject mode change", ensure(fresh, 1u), -3);
  ok &= expect_equal("mode mismatch never rewritten", read(identity), payload);
  const auto old_revision = root / "old-revision";
  write(old_revision / "world_generation.json",
      R"({"version":1,"generator":"octaryn.basegame","revision":1,"seed":1337,"mode":0})");
  const auto old_payload = read(old_revision / "world_generation.json");
  ok &= expect_equal("reject old revision", ensure(old_revision), -3);
  ok &= expect_equal("old revision unchanged", read(old_revision / "world_generation.json"), old_payload);
  const auto corrupt = root / "corrupt";
  write(corrupt / "world_generation.json", "{}");
  ok &= expect_equal("reject incomplete identity", ensure(corrupt), -3);
  for (const std::string artifact : {"world_blocks.json", "world_meta.json", "world_time.json",
                                    "player_0.json", "chunk_-32_64.json"}) {
    const auto old = root / ("old-" + artifact);
    write(old / artifact, "original save bytes");
    ok &= expect_equal("reject unversioned save", ensure(old), -4);
    ok &= expect_equal("unversioned save unchanged", read(old / artifact), std::string("original save bytes"));
    ok &= expect_equal("no identity for old save", std::filesystem::exists(old / "world_generation.json"), false);
  }
  const auto external_players = root / "external-players";
  write(external_players / "player_1.json", "old position");
  ok &= expect_equal("reject old separate player directory", ensure(root / "new-world", 0u, external_players), -4);
  for (uint32_t mode : {1u, 2u}) {
    const auto fixture = root / ("fixture-" + std::to_string(mode));
    ok &= expect_equal("fresh fixture identity", ensure(fixture, mode), 0);
    ok &= expect_equal("reload fixture identity", ensure(fixture, mode), 0);
    ok &= expect_equal("natural cannot load fixture", ensure(fixture), -3);
  }
  std::error_code error;
  std::filesystem::remove_all(root, error);
  return ok;
}
} // namespace octaryn::tools::server_world_persistence_probe
