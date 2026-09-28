#pragma once
#include "GeometryCache.h"
#include "PageResidency.h"
#include <slang-rhi.h>
#include <memory>
#include <span>

namespace octaryn::client::rendering::virtual_geometry {
struct GeometryStreamConfig {
  std::uint32_t slots{512},workers{2},feedback_capacity{4096},upload_pages{4};
  double upload_ms{1};
};
struct GeometryStreamStats {
  ResidencyStats residency;
  std::uint64_t decoded_pages{},uploaded_pages{},uploaded_bytes{};
  std::uint32_t pinned_pages{},loading_pages{},ready_pages{},uploaded_this_pump{};
};
class GeometryStream {
public:
  GeometryStream();
  // The caller must finish submitted GPU consumers before destroying this owner.
  ~GeometryStream();
  GeometryStream(const GeometryStream&)=delete;
  GeometryStream& operator=(const GeometryStream&)=delete;
  bool initialize(rhi::IDevice*,const std::filesystem::path&,const std::string& source_hash,
                  const GeometryStreamConfig& = {});
  // Extra completion values describe external ray/pose timelines; upload/raster
  // completion is read from the actual submission fence, never assumed here.
  bool pump(rhi::ICommandEncoder*,std::span<const PageRequest>,FenceValues completed={});
  // Call after every pump, including a failed recording that submitted commands.
  // One fence timeline must cover uploads, selection and all raster consumers.
  bool submitted(rhi::IFence*,std::uint64_t value,FenceValues extra_consumers={});
  // GPU-reported page use from completed selection cuts; drives the eviction LRU.
  void touch_used(std::span<const std::uint32_t> pages);
  const GeometryAsset& asset() const;
  const PageResidency& residency() const;
  std::vector<GpuPage> page_table() const;
  rhi::IBuffer* pool() const;
  rhi::IBuffer* clusters() const;
  bool roots_ready() const;
  bool gpu_idle() const;
  GeometryStreamStats stats() const;
  const std::string& error() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
