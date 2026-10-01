#pragma once
#include "SceneCatalog.h"
#include "SceneResidency.h"
#include "GeometryTransform.h"
#include "MapGeometryCache.h"
#include "SceneHierarchy.h"
#include "SceneGeometryContext.h"
#include "SceneHierarchyDetail.h"
#include <memory>

namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
struct MapRenderer;
struct PreparedScenePart {MapModel forward;std::vector<virtual_geometry::SceneHierarchyNode> children;virtual_geometry::SceneHierarchyGeometry detail;};
struct SceneRenderPart {
  virtual_geometry::ScenePart geometry;
  virtual_geometry::SceneHierarchyNode node;
  std::uint32_t primitive{};
  std::uint32_t parent{virtual_geometry::invalid_id};
  std::vector<std::uint32_t> children;
  bool hierarchy{},expanded{},exact{};
};
class SceneAssets {
public:
  SceneAssets();
  ~SceneAssets();
  bool load(WorldRenderer&,const std::filesystem::path& catalog,const std::filesystem::path& source,std::string&);
  bool prepare(std::uint32_t part,PreparedScenePart&,std::string&) const;
  bool expand(std::uint32_t part,PreparedScenePart&,std::string&,const std::atomic_bool*) const;
  bool install_children(std::uint32_t part,PreparedScenePart&&,std::string&);
  bool prepare_detail(std::uint32_t,virtual_geometry::SceneHierarchyDetail&,PreparedScenePart&,std::string&,const std::atomic_bool*) const;
  const std::filesystem::path& hierarchy_path() const {return hierarchy_path_;}
  const SceneRenderPart& render_part(std::uint32_t id) const {return render_parts_.at(id);}
  const std::vector<std::uint32_t>& roots() const {return roots_;}
  bool hierarchical() const {return hierarchy_.complete;}
  float projected_error(std::uint32_t,const WorldCamera&,float focal) const;
  scene_geometry::Selection selection(std::uint32_t) const;
  const virtual_geometry::SceneGeometryContext& context() const {return context_;}
  std::shared_ptr<MapRenderer> create(WorldRenderer&,const scene_geometry::Selection&,PreparedScenePart&&,std::string&,bool* deferred=nullptr) const;
  void instances(MapRenderer&,std::span<const std::uint32_t>) const;
  const virtual_geometry::SceneCatalog& catalog() const {return catalog_;}
  const std::vector<scene_geometry::Part>& parts() const {return parts_;}
  const std::vector<scene_geometry::Instance>& nodes() const {return nodes_;}
  std::uint64_t texture_bytes() const;
private:
  virtual_geometry::SceneCatalog catalog_;
  std::filesystem::path catalog_path_,source_;
  std::unique_ptr<MapRenderer> materials_;
  std::vector<virtual_geometry::GeometryTransform> transforms_;
  std::vector<scene_geometry::Part> parts_;
  std::vector<scene_geometry::Instance> nodes_;
  std::vector<std::vector<std::uint32_t>> mesh_nodes_;
  std::shared_ptr<void> page_scheduler_;
  virtual_geometry::SceneHierarchy hierarchy_;
  std::filesystem::path hierarchy_path_;
  std::vector<SceneRenderPart> render_parts_;
  std::vector<std::uint32_t> roots_;
  virtual_geometry::SceneGeometryContext context_;
  std::shared_ptr<virtual_geometry::SceneMemoryLease> textures_allocation_;
  bool load_render_parts(std::string&);
  bool initialize_shared(WorldRenderer&,std::string&);
  std::uint32_t append_part(std::uint32_t,const virtual_geometry::SceneHierarchyNode&);
};
}
