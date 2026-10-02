#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace octaryn::scene_loading {
struct Resource {std::filesystem::path path;std::string hash;std::uint64_t bytes{};bool tree_hash{};};
struct Primitive {std::uint32_t mesh{},primitive{},material{UINT32_MAX};std::uint64_t vertices{},triangles{};};
struct Instance {std::uint32_t node{},mesh{};std::string name;std::array<float,16> transform{};};
struct Snapshot {
    std::filesystem::path source,catalog;
    std::string identity;
    std::uint32_t meshes{},materials{};
    std::uint64_t unique_triangles{},instanced_triangles{};
    std::vector<Resource> resources;
    std::vector<Primitive> primitives;
    std::vector<Instance> instances;
    std::uint64_t retained_bytes{};
};
}
