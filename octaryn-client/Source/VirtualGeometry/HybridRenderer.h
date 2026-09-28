#pragma once
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
#include <cstdint>
#include <span>
namespace octaryn::client::rendering::virtual_geometry {
struct HybridInputs {
  rhi::IBuffer *clusters{},*pool{},*page_table{},*selected{},*counters{},*materials{},*dispatch{};
  std::uint32_t width{},height{},selected_capacity{},pool_slots{};
  std::array<float,20> view{};
  std::array<float,4> ambient{};
  rhi::IBuffer* occlusion_flags{};
  std::uint32_t occlusion_phase{};
};
// One instance per in-flight frame; callers retire all consumers before resizing/reuse.
class HybridRenderer {
public:
  bool initialize(rhi::IDevice*,const char* shader_directory,std::span<const rhi::Format> targets,rhi::Format depth);
  bool resize(rhi::IDevice*,std::uint32_t width,std::uint32_t height);
  bool visibility(rhi::ICommandEncoder*,const HybridInputs&,bool clear=true);
  bool resolve(rhi::IRenderPassEncoder*,const HybridInputs&);
  rhi::IBuffer* visibility_buffer() const {return visibility_.get();}
private:
  Slang::ComPtr<rhi::IBuffer> visibility_;
  Slang::ComPtr<rhi::IComputePipeline> clear_,software_;
  Slang::ComPtr<rhi::IRenderPipeline> hardware_,resolve_;
  std::uint32_t width_{},height_{};
  bool bind(rhi::IShaderObject*,const HybridInputs&,bool resolve);
};
}
