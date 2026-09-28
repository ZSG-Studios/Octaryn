#pragma once
#include "Selection.h"
#include <slang-rhi.h>
#include <memory>

namespace octaryn::client::rendering::virtual_geometry {
struct SelectionGpuFrame {
  std::uint32_t slot{invalid_id},generation{};
  rhi::IBuffer *selected{},*counters{},*dispatch{},*page_table{};
};
struct SelectionFeedback {
  std::uint32_t selected{},feedback_overflow{},selected_overflow{},missing_roots{};
  std::vector<PageRequest> requests;
};
class SelectionGpu {
public:
  SelectionGpu();
  ~SelectionGpu();
  SelectionGpu(const SelectionGpu&)=delete;
  SelectionGpu& operator=(const SelectionGpu&)=delete;
  bool initialize(rhi::IDevice*,const SelectionTopology&,const char* shader_path,
                  std::uint32_t feedback_capacity,std::uint32_t frame_count=3);
  // Caller submits even on a recording failure before destroying this owner.
  bool record(rhi::ICommandEncoder*,std::span<const GpuPage>,const SelectionView&,SelectionGpuFrame&);
  // Fence must cover every consumer of the returned buffers, including rendering.
  bool submitted(const SelectionGpuFrame&,rhi::IFence*,std::uint64_t value);
  // No wait: false + empty error means no completed feedback is available.
  bool poll_feedback(SelectionFeedback&);
  std::uint64_t gpu_bytes() const;
  const std::string& error() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
