#pragma once
#include "GeometryStream.h"
#include "SceneGeometryPool.h"
#include "SceneMemoryLedger.h"
#include "octaryn_native_schedule_runtime.h"
#include <slang-com-ptr.h>
#include <atomic>

namespace octaryn::client::rendering::virtual_geometry {
struct GeometryStream::State {
  struct Job {
    void* task{};
    std::filesystem::path path;
    GeometryPage descriptor;
    std::uint32_t page{invalid_id};
    PageHandle handle;
    std::vector<std::uint8_t> decoded;
    std::atomic_bool cancelled{};
    std::string error;
    bool ready{},success{};
    static int execute(void*) noexcept;
  };
  Slang::ComPtr<rhi::IDevice> device;
  std::shared_ptr<SceneMemoryLease> metadata_allocation;
  Slang::ComPtr<rhi::IBuffer> pool,clusters;
  Slang::ComPtr<rhi::IFence> fence;
  std::uint64_t signal{};
  FenceValues extra_required,extra_completed;
  GeometryAsset asset;
  GeometryStreamConfig config;
  GeometryStreamStats stats;
  std::filesystem::path path;
  std::unique_ptr<PageResidency> residency;
  std::vector<bool> pinned;
  std::vector<std::uint32_t> page_payload_bytes;
  std::vector<PageRequest> roots;
  std::vector<std::unique_ptr<Job>> jobs;
  std::vector<PageHandle> recorded_uploads,recorded_references;
  std::shared_ptr<void> scheduler;
  SceneGeometryHandle scene_asset;
  std::string error;
  bool recorded{},failed{},admission_rejected{};
  ~State();
  bool fail(std::string message) {error=std::move(message);failed=true;return false;}
  bool poll_jobs();
  bool start_jobs();
};
}
