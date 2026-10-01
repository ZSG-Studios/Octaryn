#pragma once
#include <array>
#include <cstdint>
#include <memory>

namespace octaryn::client::rendering::virtual_geometry {
enum class SceneMemoryDomain {Pages,Metadata,Selection,Materials,RayGeometry,RayScratch,WorldRay,Count};
enum class SceneMemoryPhase {Resident,Pending,Compacting,Retired,Count};
struct SceneMemoryStats {
  std::uint64_t limit{},used{},peak{};
  std::array<std::uint64_t,static_cast<unsigned>(SceneMemoryDomain::Count)> domain_bytes{};
  std::array<std::uint64_t,static_cast<unsigned>(SceneMemoryPhase::Count)> phase_bytes{};
};
struct SceneMemoryState;
class SceneMemoryLease {
public:
  ~SceneMemoryLease();
  SceneMemoryLease(const SceneMemoryLease&)=delete;
  SceneMemoryLease& operator=(const SceneMemoryLease&)=delete;
  // Grow before allocation; shrink only after the replaced resource is released.
  bool resize(std::uint64_t);
  std::shared_ptr<SceneMemoryLease> split(std::uint64_t,SceneMemoryPhase);
  void phase(SceneMemoryPhase);
  std::uint64_t bytes() const;
private:
  friend class SceneMemoryLedger;
  SceneMemoryLease(std::shared_ptr<SceneMemoryState>,std::uint64_t,SceneMemoryDomain,SceneMemoryPhase);
  std::shared_ptr<SceneMemoryState> state_;
  std::uint64_t bytes_{};
  SceneMemoryDomain domain_;
  SceneMemoryPhase phase_;
};
class SceneMemoryLedger {
public:
  explicit SceneMemoryLedger(std::uint64_t limit);
  std::shared_ptr<SceneMemoryLease> reserve(std::uint64_t,SceneMemoryDomain,
      SceneMemoryPhase=SceneMemoryPhase::Resident);
  SceneMemoryStats stats() const;
  bool owns(const std::shared_ptr<SceneMemoryLease>&) const;
private:
  std::shared_ptr<SceneMemoryState> state_;
};
}
