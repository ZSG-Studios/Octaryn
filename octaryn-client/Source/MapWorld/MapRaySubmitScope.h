#pragma once
#include <slang-rhi.h>
namespace octaryn::client::rendering {
enum class MapRaySubmitKind {Build=1,Compaction=2};
// Borrowed only while an owner records its independently submitted AS commands.
struct MapRaySubmitScope {
  void* context{};
  bool (*start)(void*,rhi::ICommandEncoder*,MapRaySubmitKind){};
  bool (*stop)(void*,rhi::ICommandEncoder*){};
  bool begin(rhi::ICommandEncoder* commands,MapRaySubmitKind kind) const {
    return !start || start(context,commands,kind);
  }
  bool end(rhi::ICommandEncoder* commands) const {return !stop || stop(context,commands);}
};
}
