#pragma once
#include <cstdint>
namespace octaryn::client::rendering {
struct WorldRetirementProgress {
  std::uint64_t owners{},pending_count{},pending_buffer_bytes{};
  std::uint64_t remaining() const {return owners?owners:(pending_count?1:0);}
};
// Completed frames alone cannot prove a blocked resource worker is progressing.
class WorldRetirementGuard {
  WorldRetirementProgress previous_;
  std::uint64_t changed_ns_{};
  bool initialized_{};
public:
  bool observe(const WorldRetirementProgress& progress,std::uint64_t now_ns,std::uint64_t timeout_ns) {
    if(!initialized_) {
      previous_=progress;changed_ns_=now_ns;initialized_=true;return true;
    }
    if(progress.owners<previous_.owners || progress.pending_count<previous_.pending_count ||
        progress.pending_buffer_bytes<previous_.pending_buffer_bytes)changed_ns_=now_ns;
    previous_=progress;
    return now_ns>=changed_ns_ && now_ns-changed_ns_<timeout_ns;
  }
};
}
