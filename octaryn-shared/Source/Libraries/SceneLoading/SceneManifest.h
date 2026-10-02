#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace octaryn::scene_loading {
struct ManifestFile {std::string path,sha256;std::uint64_t bytes{};};
struct ManifestCounts {std::uint64_t meshes{},uniqueTriangles{},instances{},instancedTriangles{};};
struct Manifest {
    std::uint32_t version{};std::string format,scene;
    std::vector<ManifestFile> files;bool prepared{};ManifestCounts counts;
};
}
