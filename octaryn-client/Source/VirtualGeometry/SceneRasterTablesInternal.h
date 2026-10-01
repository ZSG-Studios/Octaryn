#pragma once
#include "SceneRasterTables.h"
#include "SceneMemoryLedger.h"
#include <slang-com-ptr.h>
#include <array>

namespace octaryn::client::rendering::virtual_geometry {
struct SceneRasterInstance {
  std::array<float,12> world,normal;
  float orientation{};
  std::uint32_t padding[3]{};
};
struct SceneRasterDraw {
  std::uint32_t cluster{},transform{},page_base{},material_base{};
  std::uint32_t generation{},padding[3]{};
};
static_assert(sizeof(SceneRasterInstance)==112 && sizeof(SceneRasterDraw)==32);
struct SceneRasterTables::State {
  struct Bank {
    std::shared_ptr<SceneMemoryLease> allocation;
    Slang::ComPtr<rhi::IBuffer> clusters,pages,instances,draws,selected,counters,dispatch,readback,domains;
    Slang::ComPtr<rhi::IFence> fence;
    std::uint64_t signal{};
    std::uint32_t generation{};
    bool pending_feedback{};
  };
  struct Domain {
    std::uint32_t cluster_base{},page_base{},instance_base{},draw_base{},instances{},clusters{},pages{},material{};
    std::uint32_t root_cut{},padding[3]{};
    bool appended{};
  };
  Slang::ComPtr<rhi::IDevice> device;
  std::shared_ptr<SceneMemoryLedger> ledger;
  Slang::ComPtr<rhi::IComputePipeline> append,roots,finish;
  std::array<std::shared_ptr<Bank>,2> banks;
  std::vector<std::shared_ptr<Bank>> retired;
  std::vector<Domain> domains;
  SceneRasterCapacity capacity;
  rhi::ICommandEncoder* encoder{};
  std::uint32_t active{},generation{};
  std::string error;
  bool sealed{},admission_rejected{};
  bool fail(const std::string& text) {error=text;return false;}
  bool complete(const Bank&) const;
  bool collect();
  bool poll(Bank&);
  bool buffer(Slang::ComPtr<rhi::IBuffer>&,std::uint64_t,unsigned,bool readback=false);
  std::shared_ptr<Bank> allocate(SceneRasterCapacity);
};
}
