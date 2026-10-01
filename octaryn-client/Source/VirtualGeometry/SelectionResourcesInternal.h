#pragma once
#include "SelectionResources.h"
#include "SceneMemoryLedger.h"
#include "InstanceSelection.h"
#include <slang-com-ptr.h>
#include <array>
#include <atomic>
#include <unordered_map>

namespace octaryn::client::rendering::virtual_geometry {
struct SelectionScratch {
  Slang::ComPtr<rhi::IBuffer> groups,clusters,group_pages,parents;
  Slang::ComPtr<rhi::IBuffer> pages,active,requested,priorities,feedback,selected,counters,dispatch,readback,used,instances;
};
struct SelectionFeedbackMemory {std::atomic<std::uint64_t> used{};std::uint64_t limit{};};
struct SharedSelectionTicket {
  std::uint64_t owner{},offset{};
  std::uint32_t slot{},generation{},pages{},capacity{},clusters{};
  std::shared_ptr<SelectionFeedbackMemory> memory;
  std::vector<std::uint32_t> feedback;
  bool submitted{},consumed{},ready{};
  ~SharedSelectionTicket() {if(memory)memory->used.fetch_sub(feedback.size()*sizeof(std::uint32_t));}
};
struct SharedSelectionRecording {
  SelectionScratch* buffers{};
  const std::array<Slang::ComPtr<rhi::IComputePipeline>,5>* pipelines{};
  std::shared_ptr<SharedSelectionTicket> ticket;
};
struct SelectionResources::State {
  struct Bank {
    SelectionScratch buffers;
    Slang::ComPtr<rhi::IFence> fence;
    std::vector<std::weak_ptr<SharedSelectionTicket>> tickets;
    rhi::ICommandEncoder* encoder{};
    std::uint64_t signal{},readback_used{};
    std::uint32_t generation{};
  };
  Slang::ComPtr<rhi::IDevice> device;
  std::shared_ptr<SceneMemoryLease> allocation;
  std::array<Slang::ComPtr<rhi::IComputePipeline>,5> pipelines;
  std::vector<Bank> banks;
  std::shared_ptr<SelectionFeedbackMemory> feedback_memory=std::make_shared<SelectionFeedbackMemory>();
  SelectionResourcesConfig config;
  std::uint64_t next_owner{};
  std::uint64_t reserved_feedback{};
  std::unordered_map<std::uint64_t,std::uint64_t> owners;
  std::string error;
  bool fail(const char* value) {error=value;return false;}
  bool buffer(Slang::ComPtr<rhi::IBuffer>&,std::uint64_t,unsigned,bool readback=false);
  bool collect(Bank&);
};
}
