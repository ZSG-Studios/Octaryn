#pragma once
#include <filesystem>

namespace octaryn::content {
// Keep stored identities portable; expand only the path passed to native file I/O.
inline std::filesystem::path file_io_path(const std::filesystem::path& path) {
#ifdef _WIN32
  auto value=std::filesystem::absolute(path).lexically_normal().make_preferred().native();
  if(value.starts_with(L"\\\\?\\"))return value;
  if(value.starts_with(L"\\\\"))return L"\\\\?\\UNC\\"+value.substr(2);
  return L"\\\\?\\"+value;
#else
  return path;
#endif
}
inline std::filesystem::path canonical_file_path(const std::filesystem::path& path) {
  const auto resolved=std::filesystem::weakly_canonical(file_io_path(path));
#ifdef _WIN32
  const auto& value=resolved.native();
  if(value.starts_with(L"\\\\?\\UNC\\"))return L"\\\\"+value.substr(8);
  if(value.starts_with(L"\\\\?\\"))return value.substr(4);
#endif
  return resolved;
}
}
