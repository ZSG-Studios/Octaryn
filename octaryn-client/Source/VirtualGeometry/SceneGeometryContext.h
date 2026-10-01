#pragma once
#include <memory>

namespace octaryn::client::rendering::virtual_geometry {
class SceneMemoryLedger;
class SceneMemoryLease;
class SceneGeometryPool;
class SelectionResources;
class SceneRayScheduler;
class SceneRasterTables;
struct SceneGeometryContext {
  std::shared_ptr<SceneMemoryLedger> ledger;
  std::shared_ptr<SceneGeometryPool> pages;
  std::shared_ptr<SelectionResources> selection;
  std::shared_ptr<SceneRayScheduler> ray;
  std::shared_ptr<SceneRasterTables> raster;
};
}
