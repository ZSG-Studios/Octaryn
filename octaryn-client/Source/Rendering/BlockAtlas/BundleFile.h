#pragma once

#include <cstdio>
#include <string>

namespace octaryn::client::rendering {

void log_block_atlas_line(FILE *log, const char *message);
bool read_block_atlas_text_file(const char *path, const char *failure_label,
                                FILE *log, std::string &payload);
bool block_atlas_ends_with(const std::string &value,
                           const std::string &suffix);
bool find_first_block_atlas_bundle_file(const char *relative_directory,
                                        const char *filename_suffix,
                                        const char *failure_label, FILE *log,
                                        std::string &path);

} // namespace octaryn::client::rendering
