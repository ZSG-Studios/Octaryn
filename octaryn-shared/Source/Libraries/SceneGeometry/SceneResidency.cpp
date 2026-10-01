#include "SceneResidency.h"
#include "SceneSpatialIndex.h"
#include <map>
#include <set>

namespace octaryn::scene_geometry {
struct ResidencyIndex::State {
  struct Mesh {std::vector<std::uint32_t> parts;SpatialIndex index;Bounds bounds;};
  std::vector<Part> parts;
  std::vector<Instance> instances;
  std::map<std::uint32_t,Mesh> meshes;
  SpatialIndex index;
};
ResidencyIndex::ResidencyIndex():state_(std::make_unique<State>()) {}
ResidencyIndex::~ResidencyIndex()=default;
ResidencyIndex::ResidencyIndex(ResidencyIndex&&) noexcept=default;
ResidencyIndex& ResidencyIndex::operator=(ResidencyIndex&&) noexcept=default;
bool ResidencyIndex::reset(std::span<const Part> parts,std::span<const Instance> instances,std::string& error) {
  auto next=std::make_unique<State>();
  if(parts.size()>1000000 || instances.size()>1000000) {error="scene spatial metadata exceeds limits";return false;}
  next->parts.assign(parts.begin(),parts.end());next->instances.assign(instances.begin(),instances.end());
  for(std::uint32_t i=0;i<parts.size();++i) {
    const auto& part=parts[i];
    if(!bounds::valid(part.bounds) || !part.triangle_count || (part.cooked && !part.reservation_bytes)) {
      error="scene part spatial metadata is invalid";return false;
    }
    next->meshes[part.mesh].parts.push_back(i);
  }
  for(auto& [mesh,value]:next->meshes) {
    std::vector<Bounds> boxes;boxes.reserve(value.parts.size());value.bounds=bounds::empty();
    for(const auto part:value.parts) {boxes.push_back(parts[part].bounds);bounds::include(value.bounds,parts[part].bounds);}
    value.index.reset(boxes);
  }
  std::vector<Bounds> boxes;std::set<std::uint32_t> nodes;
  for(const auto& instance:instances) {
    std::array<float,16> inverse;
    if(!bounds::valid(instance.bounds) || !nodes.insert(instance.node).second ||
        !next->meshes.contains(instance.mesh) || !bounds::inverse(instance.transform,inverse)) {
      error="scene instance spatial metadata is invalid";return false;
    }
    // Derive index bounds from part metadata, not potentially stale caller bounds.
    const auto box=bounds::transform(next->meshes[instance.mesh].bounds,instance.transform);
    if(!bounds::valid(box)) {error="scene transformed bounds overflow";return false;}
    boxes.push_back(box);
  }
  next->index.reset(boxes);state_=std::move(next);error.clear();return true;
}
Plan ResidencyIndex::plan(const Query& query,std::span<const Selection> current) const {
  Plan result;const auto& s=*state_;
  for(const auto point:{query.camera,query.actor})for(const float value:point)
    if(!std::isfinite(value)) {result.error="scene query position is not finite";return result;}
  if(!std::isfinite(query.load_radius) || !std::isfinite(query.keep_radius) || !std::isfinite(query.actor_radius) ||
      query.load_radius<=0 || query.keep_radius<query.load_radius || query.actor_radius<0 || !query.budget_bytes) {
    result.error="scene residency budget is invalid";return result;
  }
  std::map<std::uint32_t,std::set<std::uint32_t>> wanted,keep;
  if((query.region && !bounds::valid(*query.region)) || (query.keep_region && !bounds::valid(*query.keep_region)) ||
      (query.keep_region && !query.region)) {result.error="scene query region is invalid";return result;}
  if(query.region && query.keep_region)for(unsigned axis=0;axis<3;++axis)
    if((*query.keep_region)[axis]>(*query.region)[axis] || (*query.keep_region)[axis+3]<(*query.region)[axis+3]) {
      result.error="scene keep region must contain its required region";return result;
    }
  const auto gather_bounds=[&](const Bounds& world_box,auto& selected) {
    s.index.query(world_box,[&](std::uint32_t id) {
      const auto& instance=s.instances[id];const auto& mesh=s.meshes.at(instance.mesh);
      mesh.index.query(world_box,instance.transform,[&](std::uint32_t local_part) {
        const auto part=mesh.parts[local_part];
        if(bounds::overlap(bounds::transform(s.parts[part].bounds,instance.transform),world_box))selected[part].insert(id);
      });
    });
  };
  const auto gather=[&](const std::array<float,3>& point,float radius,auto& selected) {
    auto world_box=bounds::around(point,radius);
    if(query.ignore_vertical) {world_box[1]=-std::numeric_limits<float>::max();world_box[4]=std::numeric_limits<float>::max();}
    s.index.query(world_box,[&](std::uint32_t id) {
      const auto& instance=s.instances[id];const auto& mesh=s.meshes.at(instance.mesh);
      mesh.index.query(world_box,instance.transform,[&](std::uint32_t local_part) {
        const auto part=mesh.parts[local_part];
        if(bounds::near(bounds::transform(s.parts[part].bounds,instance.transform),point,radius,query.ignore_vertical))selected[part].insert(id);
      });
    });
  };
  if(query.region)gather_bounds(*query.region,wanted);
  else {gather(query.camera,query.load_radius,wanted);gather(query.actor,query.actor_radius,wanted);}
  for(const auto& [id,instances]:wanted) {
    result.wanted.push_back({id,{instances.begin(),instances.end()}});
    const auto& part=s.parts[id];
    if(!part.cooked || !part.bounds_prepared)result.pending_parts.push_back(id);
    if(part.reservation_bytes>UINT64_MAX-result.reservation_bytes) {result.error="scene reservation overflow";return result;}
    result.reservation_bytes+=part.reservation_bytes;
  }
  if(!result.pending_parts.empty()) {result.error="scene neighborhood preparation is incomplete";return result;}
  if(result.reservation_bytes>query.budget_bytes) {result.error="complete scene neighborhood exceeds residency budget";return result;}
  if(query.region)gather_bounds(query.keep_region.value_or(*query.region),keep);
  else {gather(query.camera,query.keep_radius,keep);gather(query.actor,query.actor_radius+8,keep);}
  for(const auto& active:current) {
    if(active.part>=s.parts.size())continue;
    const auto found=keep.find(active.part);if(found==keep.end())continue;
    const auto& part=s.parts[active.part];
    const bool already_wanted=wanted.contains(active.part);
    if(!part.cooked || !part.bounds_prepared || (!already_wanted && part.reservation_bytes>query.budget_bytes-result.reservation_bytes))continue;
    Selection retained{active.part,{}};
    for(const auto instance:active.instances)
      if(found->second.contains(instance) && (!already_wanted || !wanted.at(active.part).contains(instance)))retained.instances.push_back(instance);
    if(retained.instances.empty())continue;
    if(!already_wanted)result.reservation_bytes+=part.reservation_bytes;
    result.retained.push_back(std::move(retained));
  }
  result.admitted=true;return result;
}
}
