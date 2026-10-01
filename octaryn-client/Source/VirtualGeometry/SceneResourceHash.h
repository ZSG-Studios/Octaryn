#pragma once
#include <filesystem>
#include <atomic>
#include <string>
namespace octaryn::client::rendering::virtual_geometry {
// Versioned SHA256 chunk tree; hashing a large source needs at most one MiB of payload memory.
std::string scene_resource_hash(const std::filesystem::path&,std::string& error,const std::atomic_bool* cancel=nullptr);
}
