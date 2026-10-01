#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace octaryn::content {
std::string resource_digest(std::span<const std::uint8_t>);
std::string resource_digest_parts(std::span<const std::span<const std::uint8_t>>);
std::string resource_file_digest(const std::filesystem::path&,std::string& error,
    std::uint64_t maximum_bytes=512ull*1024*1024,const std::atomic_bool* cancel=nullptr);
// The existing scene-source-sha256-tree-v1 format is a versioned source identity.
std::string resource_tree_digest(const std::filesystem::path&,std::string& error,
    std::uint64_t maximum_bytes=1ull<<40,const std::atomic_bool* cancel=nullptr);
}
