#pragma once
#include <filesystem>
#include <vector>
#include <cstdint>
namespace octaryn::client::world_presentation::item_files {
bool read(const std::filesystem::path&,void*,std::size_t exact_size);
void write(const std::filesystem::path&,const void*,std::size_t size);
}
