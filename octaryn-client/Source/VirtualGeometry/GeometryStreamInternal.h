#pragma once
#include "GeometryStream.h"
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
  std::vector<PageRequest> roots;
  std::vector<std::unique_ptr<Job>> jobs;
  std::vector<PageHandle> recorded_uploads,recorded_references;
  void* scheduler{};
  std::string error;
  bool recorded{},failed{};
  ~State();
  bool fail(std::string message) {error=std::move(message);failed=true;return false;}
  bool poll_jobs();
  bool start_jobs();
};
}
