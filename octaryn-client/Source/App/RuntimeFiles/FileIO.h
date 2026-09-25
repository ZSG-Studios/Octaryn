#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace octaryn_client_app {

bool read_text_file(const char *path, const char *failure_label,
                    std::string &payload);
bool read_binary_file(const char *path, const char *failure_label,
                      std::vector<uint8_t> &payload);
bool write_text_file_atomic(const std::filesystem::path &path,
                            const std::string &payload,
                            const char *failure_label);

} // namespace octaryn_client_app
