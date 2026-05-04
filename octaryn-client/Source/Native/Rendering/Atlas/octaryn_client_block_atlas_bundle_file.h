#pragma once

#include <cstdio>
#include <string>

namespace octaryn::client::rendering {

void client_block_atlas_log_line(FILE *log, const char *message);
bool client_block_atlas_read_text_file(const char *path,
                                       const char *failure_label, FILE *log,
                                       std::string &payload);
bool client_block_atlas_ends_with(const std::string &value,
                                  const std::string &suffix);
bool client_block_atlas_find_first_bundle_file(const char *relative_directory,
                                               const char *filename_suffix,
                                               const char *failure_label,
                                               FILE *log, std::string &path);

} // namespace octaryn::client::rendering
