#include "ScenePreparationHierarchyInternal.h"
#include "SceneHierarchyInternal.h"
#include "ScenePreparationLock.h"
#include "SceneOrder.h"
#include "FilePath.h"
#include <stdexcept>

namespace octaryn::client::rendering::virtual_geometry {
namespace {
void require(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
bool same_source(const SceneCatalog& original,SceneCatalog layout) {
  if(original.source!=layout.source || original.resources.size()!=layout.resources.size() ||
      original.primitives.size()!=layout.primitives.size())return false;
  for(unsigned i=0;i<original.resources.size();++i) {
    const auto& a=original.resources[i];const auto& b=layout.resources[i];
    if(a.path!=b.path || a.hash!=b.hash || a.bytes!=b.bytes)return false;
  }
  for(unsigned i=0;i<original.primitives.size();++i) {
    layout.primitives[i].triangle_order=original.primitives[i].triangle_order;
    layout.primitives[i].triangle_order_hash=original.primitives[i].triangle_order_hash;
  }
  return hierarchy_layout_identity(original,{})==hierarchy_layout_identity(layout,{});
}
}
std::filesystem::path prepare_world_layout(const ScenePreparationRequest& request,SceneCatalog& catalog,
    const std::atomic_bool* cancel,const ScenePreparationNotify& notify) {
  const auto path=request.catalog.parent_path()/"world-layout.json";std::string error;
  {
    ScenePreparationLock lock(path);
    if(std::filesystem::exists(content::file_io_path(path))) {
      SceneCatalog previous;require(read_scene_catalog(path,previous,error),error);
      require(same_source(catalog,previous),"prepared spatial layout belongs to a different original source or node set");
    }else require(write_scene_catalog(path,catalog,error),error);
  }
  // One source validation and reader for the whole request, including resumed orders.
  if(notify)notify({ScenePreparationStage::Layout});
  require(prepare_scene_spatial_order(path,0,UINT64_MAX,error,cancel,true,[&](auto completed,auto total) {
    if(notify)notify({ScenePreparationStage::Layout,completed,total,catalog.parts.size()});
  }),error);
  require(read_scene_catalog(path,catalog,error),error);return path;
}
}
