#pragma once
#include "SceneGeometryPool.h"
#include "SceneMemoryLedger.h"
#include "SceneRootPages.h"
#include "octaryn_native_schedule_runtime.h"
#include <slang-com-ptr.h>
#include <atomic>
#include <unordered_map>

namespace octaryn::client::rendering::virtual_geometry {
struct SceneGeometryPool::State {
  struct Asset {
    std::uint32_t generation{},users{};
    std::string hash;
    std::filesystem::path path;
    std::vector<GeometryPage> descriptors;
    std::vector<std::uint32_t> pages,roots;
    std::uint64_t last_signal{},recorded_epoch{};
    bool retiring{};
  };
  struct Owner {
    SceneGeometryHandle asset;std::uint32_t local{};
    SceneRootSpan root;
    std::uint64_t upload_signal{},consumer_signal{};
    bool loading{};
  };
  struct Job {
    void* task{};
    SceneGeometryHandle asset;
    std::filesystem::path path;
    GeometryPage descriptor;
    PageHandle handle;
    std::uint32_t page{invalid_id};
    SceneRootSpan root;
    std::vector<std::uint8_t> decoded;
    std::atomic_bool cancelled{};
    std::string error;
    bool ready{},success{};
    static int execute(void*) noexcept;
  };
  Slang::ComPtr<rhi::IDevice> device;
  Slang::ComPtr<rhi::IBuffer> pool;
  Slang::ComPtr<rhi::IFence> fence;
  std::shared_ptr<SceneMemoryLease> allocation;
  std::shared_ptr<void> scheduler;
  SceneGeometryPoolConfig config;
  SceneGeometryPoolStats counters;
  std::unique_ptr<PageResidency> residency;
  std::unique_ptr<SceneRootPages> root_pages;
  std::vector<Asset> assets;
  std::vector<Owner> owners;
  std::vector<std::uint32_t> free_pages;
  std::unordered_map<std::string,SceneGeometryHandle> hashes;
  std::vector<std::unique_ptr<Job>> jobs;
  std::vector<PageHandle> uploads,references;
  std::vector<std::uint32_t> root_uploads,root_references;
  std::vector<std::uint32_t> reference_generations;
  std::vector<SceneGeometryHandle> consumers;
  rhi::ICommandEncoder* encoder{};
  std::uint64_t signal{},completed{},epoch{};
  std::uint32_t pinned{};
  std::string error;
  bool failed{},admission_rejected{};
  ~State();
  Asset* find(SceneGeometryHandle);
  const Asset* find(SceneGeometryHandle) const;
  bool fail(std::string message) {error=std::move(message);failed=true;admission_rejected=false;return false;}
  bool reject(std::string message) {error=std::move(message);admission_rejected=true;return false;}
  bool poll();
  bool start_jobs();
  void collect();
};
}
