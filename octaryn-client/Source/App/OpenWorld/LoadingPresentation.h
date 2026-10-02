#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>

namespace octaryn::client::app {
// Main-thread loading observations span renderer replacements and reset for
// each request. Frame/capture counters never gate product presentation.
struct LoadingPresentation {
  std::uint64_t epoch{},frames{},started{},last{},last_capture{};
  unsigned captures{};
  std::unordered_set<std::string> stages;
  void begin() {
    ++epoch;frames=started=last=last_capture=0;captures=0;stages.clear();
  }
};
inline LoadingPresentation& loading_presentation() {
  static LoadingPresentation value;return value;
}
}
