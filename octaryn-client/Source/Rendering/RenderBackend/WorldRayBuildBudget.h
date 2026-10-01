#pragma once
#include <algorithm>
#include <cstdint>
namespace octaryn::client::rendering::world_ray {
inline constexpr unsigned BuildJobCapacity=8;
inline constexpr std::uint64_t BuildCpuBudgetNs=2000000;
struct BuildBudget {
  unsigned limit{},face_limit{},started{};
  std::uint64_t faces{};
  BuildBudget(unsigned available,unsigned max_faces)
      :limit(std::min(available,BuildJobCapacity)),face_limit(max_faces) {}
  bool allows(unsigned next_faces,std::uint64_t elapsed_ns) const {
    // An indivisible first column must make progress even above the face cap.
    return started<limit && (!started ||
        (elapsed_ns<BuildCpuBudgetNs && faces+next_faces<=face_limit));
  }
  void consumed(unsigned count) {++started;faces+=count;}
};
struct FrameBuildBudget {
  BuildBudget work{BuildJobCapacity,262144};
  std::uint64_t cpu_ns{};
  void reset(unsigned count,unsigned faces) {work=BuildBudget(count,faces);cpu_ns=0;}
  bool allows(unsigned faces,std::uint64_t current_slice_ns) const {
    return work.allows(faces,cpu_ns+current_slice_ns);
  }
  void consumed(unsigned faces) {work.consumed(faces);}
};
}
