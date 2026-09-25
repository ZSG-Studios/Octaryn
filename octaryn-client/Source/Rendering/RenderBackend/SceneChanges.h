#pragma once
#include <array>
#include <cstdint>
namespace octaryn::client::rendering {
enum class SceneChangeKind { Added, Modified, Removed, AccelerationReady };
struct SceneChange {
  std::uint64_t revision{};
  std::int32_t x{},z{},min_y{},height{};
  SceneChangeKind kind{};
  // Acceleration-ready rebuilds that changed no opaque/fluid faces
  // (sprite-only churn like torches, topology-only halo remeshes).
  bool minor_build{};
};
// Render-thread publication; each effect retains its own cursor.
class SceneChanges {
  std::array<SceneChange,256> changes_{};
  std::uint64_t revision_{};
public:
  std::uint64_t revision() const {return revision_;}
  void notify_column(std::int32_t x,std::int32_t z,int min_y,int height,SceneChangeKind kind,bool minor_build=false) {
    const auto revision=++revision_;
    changes_[(revision-1)%changes_.size()]={revision,x,z,min_y,height,kind,minor_build};
  }
  // False means the cursor expired and the consumer must invalidate all history.
  template<class Visitor> bool for_each_since(std::uint64_t cursor,Visitor visit) const {
    if(cursor>revision_ || revision_-cursor>changes_.size())return false;
    for(auto revision=cursor+1;revision<=revision_;++revision)visit(changes_[(revision-1)%changes_.size()]);
    return true;
  }
};
}
