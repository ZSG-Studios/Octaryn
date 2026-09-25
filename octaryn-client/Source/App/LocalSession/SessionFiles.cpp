#include "SessionFiles.h"
#include <fstream>
#include <iterator>
#if defined(_WIN32)
#include <windows.h>
#endif

namespace octaryn::client::app::local_session {
std::string utf8_path(const std::filesystem::path& path) {
  const auto value = path.u8string();
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
  auto temporary = path;
  temporary += ".tmp";
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(text.data(), static_cast<std::streamsize>(text.size()))) return false;
    stream.close();
    if (!stream) return false;
  }
#if defined(_WIN32)
  return MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
  std::error_code error;
  std::filesystem::rename(temporary, path, error);
  return !error;
#endif
}

bool read_text(const std::filesystem::path& path, std::string& text) {
  constexpr size_t limit = 16384;
#if defined(_WIN32)
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size{};
  bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= limit;
  if (ok) {
    text.resize(static_cast<size_t>(size.QuadPart));
    DWORD read{};
    ok = ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &read, nullptr) && read == text.size();
  }
  CloseHandle(file);
  return ok;
#else
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0 || size > limit) return false;
  std::ifstream stream(path, std::ios::binary);
  text.assign(std::istreambuf_iterator<char>(stream), {});
  return (stream.good() || stream.eof()) && text.size() == size;
#endif
}
}
