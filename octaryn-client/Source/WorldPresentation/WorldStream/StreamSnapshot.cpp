#include "StreamSnapshot.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <set>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace octaryn::client::world_presentation {
namespace {
struct SnapshotBytes {
  std::vector<char> bytes;
  std::size_t offset{};
  bool valid{true};
  void read(char* target, std::size_t count) {
    if (!valid || count > bytes.size() - offset) { valid = false; return; }
    std::memcpy(target, bytes.data() + offset, count);
    offset += count;
  }
  explicit operator bool() const { return valid; }
};
bool load_snapshot(const std::filesystem::path& path, SnapshotBytes& input) {
  constexpr std::uint64_t limit = 32 * 1024 * 1024;
#if defined(_WIN32)
  struct File {
    HANDLE handle;
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
  } file{CreateFileW(path.c_str(), GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, nullptr)};
  LARGE_INTEGER size{};
  if (file.handle == INVALID_HANDLE_VALUE || !GetFileSizeEx(file.handle, &size) ||
      size.QuadPart < 128 || static_cast<std::uint64_t>(size.QuadPart) > limit) return false;
  input.bytes.resize(static_cast<std::size_t>(size.QuadPart));
  DWORD count{};
  return ReadFile(file.handle, input.bytes.data(), static_cast<DWORD>(input.bytes.size()), &count, nullptr) &&
      count == input.bytes.size();
#else
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto size = file.tellg();
  if (!file || size < 128 || static_cast<std::uint64_t>(size) > limit) return false;
  input.bytes.resize(static_cast<std::size_t>(size));
  file.seekg(0);
  return static_cast<bool>(file.read(input.bytes.data(), static_cast<std::streamsize>(input.bytes.size())));
#endif
}
template <typename T> bool read(SnapshotBytes& input, T& value) {
  input.read(reinterpret_cast<char*>(&value), sizeof(value));
  return static_cast<bool>(input);
}
struct ColumnRecord {
  std::int32_t x{}, z{}, origin_x{}, origin_z{};
  std::uint32_t offset{}, count{};
};
void hash_value(std::uint64_t& hash, std::uint32_t value) {
  hash ^= value;
  hash *= 1099511628211ull;
}
} // namespace

bool read_stream_snapshot(const std::filesystem::path& path,
                          StreamSnapshot& snapshot, std::string& error) {
  error = "waiting_for_server_snapshot";
  if constexpr (std::endian::native != std::endian::little) {
    error = "unsupported_snapshot_endianness";
    return false;
  }
  error = "invalid_server_snapshot";
  SnapshotBytes input;
  if (!load_snapshot(path, input)) return false;
  const auto bytes = input.bytes.size();
  std::array<char, 8> magic{};
  input.read(magic.data(), magic.size());
  std::uint32_t version{}, radius{}, second{}, mode{}, ground{}, column_count{}, block_count{};
  std::int32_t center_x{}, center_z{};
  std::uint64_t epoch{}, seed{}, day{}, authoritative_revision{};
  std::uint32_t generator_mode{}, generator_revision{};
  double seconds{};
  float fraction{};
  std::array<float, 8> player{};
  if (!input || std::memcmp(magic.data(), "OCSTRM01", 8) != 0 ||
      !read(input, version) || version != 3 || !read(input, epoch) || !read(input, authoritative_revision) ||
      !read(input, center_x) || !read(input, center_z) || !read(input, radius) ||
      !read(input, seed) || !read(input, generator_mode) || !read(input, generator_revision) ||
      !read(input, day) || !read(input, second) ||
      !read(input, seconds) || !read(input, fraction)) return false;
  for (auto& value : player) if (!read(input, value) || !std::isfinite(value)) return false;
  if (!read(input, mode) || !read(input, ground) ||
      !read(input, column_count) || !read(input, block_count)) return false;
  if (seed != 1337) { error = "unsupported_server_terrain_seed"; return false; }
  if (generator_mode != 0 || generator_revision != 3) {
    error = "unsupported_server_terrain_generator";
    return false;
  }
  if (radius > 128 || column_count > 66049 || block_count > 1000000 ||
      !std::isfinite(seconds) || !std::isfinite(fraction) ||
      bytes != 128ull + 24ull * column_count + 14ull * block_count) return false;

  std::vector<ColumnRecord> records(column_count);
  std::set<std::pair<std::int32_t, std::int32_t>> seen;
  std::uint32_t offset = 0;
  for (auto& record : records) {
    if (!read(input, record.x) || !read(input, record.z) ||
        !read(input, record.origin_x) || !read(input, record.origin_z) ||
        !read(input, record.offset) || !read(input, record.count)) return false;
    if (std::abs(static_cast<std::int64_t>(record.x)) > 1000000 ||
        std::abs(static_cast<std::int64_t>(record.z)) > 1000000 ||
        static_cast<std::int64_t>(record.x) * 32 != record.origin_x ||
        static_cast<std::int64_t>(record.z) * 32 != record.origin_z ||
        std::abs(static_cast<std::int64_t>(record.x) - center_x) > radius ||
        std::abs(static_cast<std::int64_t>(record.z) - center_z) > radius ||
        record.offset != offset || record.count > block_count - offset ||
        !seen.emplace(record.x, record.z).second) return false;
    offset += record.count;
  }
  if (offset != block_count) return false;
  StreamSnapshot next{epoch, seed, {}};
  next.columns.reserve(column_count);
  for (const auto& record : records) {
    SnapshotColumn column{record.x, record.z, 1469598103934665603ull, {}};
    column.generator_revision = generator_revision;
    column.authoritative_revision = authoritative_revision;
    hash_value(column.revision, generator_revision);
    column.edits.resize(record.count);
    for (auto& edit : column.edits) {
      if (!read(input, edit.x) || !read(input, edit.y) || !read(input, edit.z) ||
          !read(input, edit.block)) return false;
      if (edit.x < record.origin_x || edit.x >= record.origin_x + 32 ||
          edit.z < record.origin_z || edit.z >= record.origin_z + 32 ||
          edit.y < StreamWorldMinY || edit.y >= StreamWorldMinY + StreamWorldHeight)
        return false;
      hash_value(column.revision, static_cast<std::uint32_t>(edit.x));
      hash_value(column.revision, static_cast<std::uint32_t>(edit.y));
      hash_value(column.revision, static_cast<std::uint32_t>(edit.z));
      hash_value(column.revision, edit.block);
    }
    next.columns.push_back(std::move(column));
  }
  if (!input) return false;
  snapshot = std::move(next);
  error.clear();
  return true;
}

} // namespace octaryn::client::world_presentation
