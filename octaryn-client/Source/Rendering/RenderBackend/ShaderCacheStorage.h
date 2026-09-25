#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace octaryn::client::rendering::shader_cache {
inline constexpr std::uint64_t Magic=0x314548434143535aull;
inline constexpr std::uint64_t MaxBlob=64ull*1024*1024,MaxBytes=256ull*1024*1024;
inline constexpr std::size_t MaxEntries=512;
struct Entry {
  std::filesystem::path path;
  std::uint64_t bytes{};
  std::filesystem::file_time_type used;
};
struct Inventory {std::vector<Entry> files;std::uint64_t bytes{};};
bool key_name(const std::filesystem::path&);
bool regular_entry(const std::filesystem::path&);
bool inventory(const std::filesystem::path&,Inventory&);
// The persistent lock file is never removed. No unlocked fallback is permitted.
class DirectoryLock {
  std::intptr_t handle{-1};
  bool held{};
public:
  explicit DirectoryLock(const std::filesystem::path&);
  ~DirectoryLock();
  DirectoryLock(const DirectoryLock&)=delete;
  DirectoryLock& operator=(const DirectoryLock&)=delete;
  explicit operator bool() const {return held;}
};
struct Temporary {
  std::filesystem::path path;
  ~Temporary();
};
bool publish(const std::filesystem::path& temporary,const std::filesystem::path& destination);
}
