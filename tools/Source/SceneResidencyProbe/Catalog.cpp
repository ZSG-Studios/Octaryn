#include "SceneCatalog.h"
#include "SceneBudget.h"
#include "SceneResidency.h"
#include "CollisionBudget.h"
#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string_view>

using namespace octaryn::client::rendering::virtual_geometry;
using namespace octaryn::scene_geometry;
namespace {
float number(const char* text) {
  float value{};const std::string_view input(text);const auto parsed=std::from_chars(input.data(),input.data()+input.size(),value);
  if(parsed.ec!=std::errc{} || parsed.ptr!=input.data()+input.size() || !std::isfinite(value))throw std::runtime_error("invalid finite query number");
  return value;
}
void inspect(const SceneCatalog& catalog,const ResidencyIndex& index,std::array<float,3> eye,float render_radius,float actor_radius) {
  Query query;query.camera=query.actor=eye;query.load_radius=query.keep_radius=render_radius;
  query.actor_radius=actor_radius;query.budget_bytes=UINT64_MAX;
  const auto render=index.plan(query);
  std::uint64_t render_pairs{},render_triangles{},known_bytes{},uncooked{};
  std::vector<std::uint64_t> nodes(catalog.mesh_count);for(const auto& node:catalog.instances)++nodes[node.mesh];
  for(const auto& selection:render.wanted) {
    const auto& part=catalog.parts[selection.part];const auto& primitive=catalog.primitives[part.primitive];
    render_pairs+=selection.instances.size();render_triangles+=part.triangle_count*selection.instances.size();
    known_bytes+=scene_part_reservation(part,primitive,nodes[primitive.mesh]);uncooked+=part.geometry.empty()?1:0;
  }
  query.load_radius=query.keep_radius=query.actor_radius=actor_radius;
  query.ignore_vertical=true;const auto collision=index.plan(query);
  query.ignore_vertical=false;const auto collision3d=index.plan(query);
  const auto count=[&](const Plan& plan) {
    std::array<std::uint64_t,3> sums{};
    for(const auto& selection:plan.wanted) {
      const auto triangles=catalog.parts[selection.part].triangle_count;
      sums[0]+=selection.instances.size();sums[1]+=triangles*selection.instances.size();
      sums[2]+=collision_part_reservation(triangles)*selection.instances.size();
    }
    return sums;
  };
  const auto xz=count(collision),xyz=count(collision3d);
  std::printf("scene_catalog_plan render_radius=%.6g actor_radius=%.6g render_parts=%zu render_pairs=%llu render_triangles=%llu uncooked=%llu pending_bounds=%zu known_render_bytes=%llu collision_xz_pairs=%llu collision_xz_triangles=%llu collision_xz_bytes=%llu collision_xyz_pairs=%llu collision_xyz_triangles=%llu collision_xyz_bytes=%llu\n",
      render_radius,actor_radius,render.wanted.size(),static_cast<unsigned long long>(render_pairs),static_cast<unsigned long long>(render_triangles),
      static_cast<unsigned long long>(uncooked),render.pending_parts.size(),static_cast<unsigned long long>(known_bytes),
      static_cast<unsigned long long>(xz[0]),static_cast<unsigned long long>(xz[1]),static_cast<unsigned long long>(xz[2]),
      static_cast<unsigned long long>(xyz[0]),static_cast<unsigned long long>(xyz[1]),static_cast<unsigned long long>(xyz[2]));
  if((!render.admitted && render.pending_parts.empty()) || (!collision.admitted && collision.pending_parts.empty()))
    throw std::runtime_error(render.error+" "+collision.error);
}
}
int main(int argc,char** argv) {
  try {
    if(argc!=5 && argc!=7)throw std::runtime_error("usage: scene_catalog_probe catalog.json x y z [render_radius actor_radius]");
    SceneCatalog catalog;std::string error;
    if(!read_scene_catalog(std::filesystem::path(reinterpret_cast<const char8_t*>(argv[1])),catalog,error))throw std::runtime_error(error);
    std::vector<Part> parts;std::vector<Instance> instances;
    for(const auto& part:catalog.parts) {
      const auto& primitive=catalog.primitives[part.primitive];
      parts.push_back({primitive.mesh,primitive.primitive,part.first_triangle,part.triangle_count,1,part.bounds,true,part.bounds_prepared});
    }
    for(const auto& node:catalog.instances)instances.push_back({node.node,node.mesh,node.transform,node.bounds});
    ResidencyIndex index;if(!index.reset(parts,instances,error))throw std::runtime_error(error);
    const std::array<float,3> eye{number(argv[2]),number(argv[3]),number(argv[4])};
    if(argc==7)inspect(catalog,index,eye,number(argv[5]),number(argv[6]));
    else for(const auto radius:std::array<std::array<float,2>,6>{{{128,24},{32,8},{8,3},{2,1},{1,.5f},{.25f,.1f}}})
      inspect(catalog,index,eye,radius[0],radius[1]);
    return 0;
  }catch(const std::exception& error){std::fprintf(stderr,"scene_catalog_probe_failed reason=%s\n",error.what());return 1;}
}
