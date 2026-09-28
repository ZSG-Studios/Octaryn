#pragma once
#include <slang-rhi.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace octaryn::client::rendering::virtual_geometry {
struct OcclusionInputs {
  rhi::IBuffer *clusters{},*selected{},*selection_counters{};
  std::uint32_t width{},height{};
  std::array<float,20> view{};
};
// Caller owns the frame fences. All operations execute on the same graphics
// queue; selected indices stay stable between the early and late visibility pass.
class OcclusionGpu {
public:
  OcclusionGpu();
  ~OcclusionGpu();
  bool initialize(rhi::IDevice*,const char* shader_path,std::uint32_t selected_capacity);
  bool begin(rhi::ICommandEncoder*,std::uint32_t frame_slot,const OcclusionInputs&);
  bool build_current(rhi::ICommandEncoder*,rhi::IBuffer* visibility);
  bool retest(rhi::ICommandEncoder*);
  bool finish(rhi::ICommandEncoder*,rhi::IBuffer* visibility);
  rhi::IBuffer* flags() const;
  rhi::IBuffer* counters() const;
  std::uint64_t gpu_bytes() const;
  const std::string& error() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
