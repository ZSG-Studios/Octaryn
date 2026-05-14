#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace octaryn_client_app {

uint32_t read_exit_after_frames();
double read_exit_after_seconds();
bool read_enabled_flag(const char *name);
bool env_value_is_present(const char *name);
bool set_process_env(const char *name, const std::filesystem::path &value);
bool set_process_env_text(const char *name, const char *value);
bool build_client_bundle_path(char *path, size_t path_size,
                              const char *relative_path,
                              const char *failure_label);

} // namespace octaryn_client_app
