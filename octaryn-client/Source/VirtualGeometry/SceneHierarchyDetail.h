#pragma once
#include "SceneHierarchy.h"
#include <memory>

namespace octaryn::client::rendering::virtual_geometry {
// One caller-owned worker retains the source reader and validated source snapshot.
class SceneHierarchyDetail {
public:
  SceneHierarchyDetail();
  ~SceneHierarchyDetail();
  bool open(const std::filesystem::path& package,std::string&,const std::atomic_bool* cancel=nullptr);
  bool prepare(std::uint32_t primitive,std::uint32_t node,SceneHierarchyGeometry&,std::string&);
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
