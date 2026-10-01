#pragma once
#include "Selection.h"
#include <slang-rhi.h>
#include <memory>

namespace octaryn::client::rendering::virtual_geometry {
struct InstanceSelectionView;
class SelectionResources;
struct SelectionGpuFrame {
  std::uint32_t slot{invalid_id},generation{};
  rhi::IBuffer *selected{},*counters{},*dispatch{},*page_table{};
  std::uint64_t owner{};
};
struct SelectionFeedback {
  std::uint32_t selected{},feedback_overflow{},selected_overflow{},missing_roots{};
  float maximum_error_pixels{};
  std::vector<PageRequest> requests;
  std::vector<std::uint32_t> used_pages;
};
class SelectionGpu {
public:
  SelectionGpu();
  ~SelectionGpu();
  SelectionGpu(const SelectionGpu&)=delete;
  SelectionGpu& operator=(const SelectionGpu&)=delete;
  bool initialize(rhi::IDevice*,const SelectionTopology&,const char* shader_path,
                  std::uint32_t feedback_capacity,std::uint32_t frame_count=3,
                  std::shared_ptr<SelectionResources> shared={});
  // Caller submits even on a recording failure before destroying this owner.
  // When timing is non-null, stamps first_query..first_query+5 bracket upload,
  // reset, the depth loop, compact, finish/feedback, and the readback copies.
  bool record(rhi::ICommandEncoder*,std::span<const GpuPage>,const SelectionView&,SelectionGpuFrame&,
              rhi::IQueryPool* timing=nullptr,std::uint32_t first_query=1);
  bool record(rhi::ICommandEncoder*,std::span<const GpuPage>,std::span<const InstanceSelectionView>,SelectionGpuFrame&,
              rhi::IQueryPool* timing=nullptr,std::uint32_t first_query=1);
  // Fence must cover every consumer of the returned buffers, including rendering.
  bool submitted(const SelectionGpuFrame&,rhi::IFence*,std::uint64_t value);
  // No wait: false + empty error means no completed feedback is available.
  bool poll_feedback(SelectionFeedback&);
  std::uint64_t gpu_bytes() const;
  const std::string& error() const;
  bool admission_rejected() const;
private:
  bool record_impl(rhi::ICommandEncoder*,std::span<const GpuPage>,const SelectionView&,
      std::span<const InstanceSelectionView>,SelectionGpuFrame&,rhi::IQueryPool*,std::uint32_t);
  struct State;
  std::unique_ptr<State> state_;
};
}
