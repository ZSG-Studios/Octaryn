#pragma once
#include "SceneSnapshot.h"
#include <atomic>
#include <functional>
#include <memory>
namespace octaryn::scene_loading {
inline constexpr std::uint64_t working_bytes=32ull*1024*1024,memory_limit=64ull*1024*1024;
// Admission estimates cover parser/snapshot work; this is not an allocator RSS limit.
// Tree digest leaf metadata stays <=1 MiB per resource and is released between files.
inline constexpr std::uint64_t resource_limit=16ull*1024*1024*1024,verification_limit=64ull*1024*1024*1024;
struct Preparation {
    std::filesystem::path root,input;
    const std::atomic_bool* cancel{};
    std::function<void(std::uint64_t,std::uint64_t)> progress;
    void check() const;
    std::filesystem::path confined(const std::filesystem::path&) const;
    std::filesystem::path relative(const std::filesystem::path& parent,const std::string&) const;
    std::string text(const std::filesystem::path&) const;
};
bool reserve_verification_work();
void release_verification_work();
void require(bool,const char*);
bool hash_valid(const std::string&);
void add(std::uint64_t&,std::uint64_t);
std::unique_ptr<Snapshot> prepare(const Preparation&);
void gltf_metadata(const Preparation&,const std::filesystem::path&,Snapshot&);
void catalog_metadata(const Preparation&,const std::filesystem::path&,Snapshot&);
void cook_identity(const Preparation&,const std::filesystem::path&,const std::string& hash,
                   std::uint64_t triangles,std::uint32_t clusters,std::uint32_t pages);
void verify_resources(const Preparation&,Snapshot&);
}
