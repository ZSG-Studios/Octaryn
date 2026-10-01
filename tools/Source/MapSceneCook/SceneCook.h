#pragma once
#include <filesystem>
#include <cstdint>
bool cook_scene_parts(const std::filesystem::path&,std::uint64_t first,std::uint64_t count);
bool test_scene_catalog(const std::filesystem::path&);
bool prepare_scene_bounds(const std::filesystem::path&,std::uint64_t first,std::uint64_t count);
int prepare_scene_neighborhood_cli(int argc,char** argv);
int qualify_scene_spawn_cli(char** argv);
