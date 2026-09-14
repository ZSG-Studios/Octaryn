#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace octaryn::client::app::local_session {
bool write_text(const std::filesystem::path& path, std::string_view text);
bool read_text(const std::filesystem::path& path, std::string& text);
std::string utf8_path(const std::filesystem::path& path);
}
