#pragma once
#include "SceneSelectionBudget.h"
#include <slang-rhi.h>
#include <memory>

namespace octaryn::client::rendering::virtual_geometry {
class SceneMemoryLedger;
struct SharedSelectionTicket;
struct SharedSelectionRecording;
// Sequential selection/visibility/resolve share scratch; feedback retains tagged frame ranges.
class SelectionResources {
public:
  SelectionResources();
  ~SelectionResources();
  bool initialize(rhi::IDevice*,std::shared_ptr<SceneMemoryLedger>,const char* shader,
      const SelectionResourcesConfig& = {});
  static std::uint64_t required_bytes(const SelectionResourcesConfig&);
  std::uint64_t register_owner(std::uint32_t pages,std::uint32_t feedback_capacity);
  void unregister_owner(std::uint64_t);
  std::uint32_t feedback_capacity(std::uint32_t pages) const;
  static std::uint64_t feedback_bytes(std::uint32_t pages,std::uint32_t capacity);
  bool supports(const SelectionTopology&,std::uint32_t feedback_capacity) const;
  bool record(rhi::ICommandEncoder*,std::uint64_t owner,const SelectionTopology&,std::uint32_t feedback_capacity,
      std::uint32_t instances,SharedSelectionRecording&);
  bool submitted(const std::shared_ptr<SharedSelectionTicket>&,rhi::IFence*,std::uint64_t);
  bool completed(const std::shared_ptr<SharedSelectionTicket>&) const;
  rhi::IDevice* device() const;
  std::uint64_t gpu_bytes() const;
  const std::string& error() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
