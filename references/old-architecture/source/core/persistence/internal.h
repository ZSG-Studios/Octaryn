#pragma once

#include <algorithm>
#include <filesystem>
#include <span>
#include <utility>

#include "core/profile.h"
#include "core/persistence/json_file_io.h"
#include "core/persistence/state.h"
#include "core/persistence/world_types.h"

namespace persistence_json {

void reset_state(persistence_state& state);
auto settings_path(const persistence_state& state) -> std::filesystem::path;
auto lighting_tuning_path(const persistence_state& state) -> std::filesystem::path;
auto world_time_path(const persistence_state& state) -> std::filesystem::path;
auto world_save_metadata_path(const persistence_state& state) -> std::filesystem::path;
auto player_path(const persistence_state& state, int id) -> std::filesystem::path;
auto world_root_path(const persistence_state& state) -> std::filesystem::path;
auto chunk_path(const persistence_state& state, int cx, int cz) -> std::filesystem::path;
auto chunk_zstd_path(const persistence_state& state, int cx, int cz) -> std::filesystem::path;
bool persistence_world_save_data_exists(const persistence_state& state);
bool persistence_ensure_world_save_metadata(const persistence_state& state);

bool compress_zstd(std::string_view input, std::vector<char>& output);
bool decompress_zstd(std::span<const char> input, std::string& output);
bool compress_lz4(std::string_view input, std::vector<char>& output);
bool decompress_lz4(const void* input, int size, std::string& output);
bool write_gzip_file(const std::filesystem::path& path, std::string_view payload);
bool read_gzip_file(const std::filesystem::path& path, std::string& payload);
bool write_chunk_override_file_atomic(const std::filesystem::path& path, const chunk_override_file& value);
bool read_chunk_override_file(const std::filesystem::path& path, chunk_override_file& value);
bool upgrade_chunk_override_file(chunk_override_file& value, bool* changed);
void load_chunk_override_into_cache(chunk_cache_entry& chunk, const chunk_override_file& file, bool dirty);
auto make_chunk_override_file(const chunk_cache_entry& chunk) -> chunk_override_file;
auto to_settings_file(const app_settings_blob& blob) -> settings_file;
auto to_settings_blob(const settings_file& file) -> app_settings_blob;
auto to_lighting_tuning_file(const lighting_tuning_blob& blob) -> lighting_tuning_file;
auto to_lighting_tuning_blob(const lighting_tuning_file& file) -> lighting_tuning_blob;
auto to_player_file(const player_blob& blob) -> player_file;
auto to_player_blob(const player_file& file) -> player_blob;
auto to_world_time_file(const world_time_blob& blob) -> world_time_file;
auto to_world_time_blob(const world_time_file& file) -> world_time_blob;
void load_all_world_chunks(persistence_state& state);
void flush_settings(persistence_state& state);
void flush_lighting_tuning(persistence_state& state);
void flush_world_time(persistence_state& state);
void flush_players(persistence_state& state);
void flush_chunks(persistence_state& state);

} // namespace persistence_json
