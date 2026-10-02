#include "SceneBodyMirror.h"
#include "SceneBodyShapes.h"
#include "MeshCollisionScene.h"
#include "MeshCollisionWorld.h"
#include "FilePath.h"
#include <glaze/glaze.hpp>
#include <box3d/collision.h>
#include <fstream>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <cmath>

namespace octaryn::client::app {
struct MirrorShapeSource {
  unsigned kind{};std::vector<float> points;
  std::array<float,3> localPosition{},halfExtents{},capsuleA{},capsuleB{};
  std::array<float,4> localRotation{0,0,0,1};float radius{};
};
struct MirrorBodySource {
  uint64_t sourceId{};std::array<float,3> position{};std::array<float,4> rotation{0,0,0,1};
  std::vector<MirrorShapeSource> shapes;
};
struct MirrorBodyCatalog {unsigned version{};std::vector<MirrorBodySource> bodies;};
namespace {
bool finite(const float* values,unsigned count) {for(unsigned i=0;i<count;++i)if(!std::isfinite(values[i]))return false;return true;}
bool valid_rotation(const float* q) {return finite(q,4) && std::abs(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]-1)<.01f;}
struct MirrorBody {
  b3BodyId id{};std::vector<b3HullData*> hulls;
  ~MirrorBody() {if(b3Body_IsValid(id))b3DestroyBody(id);for(auto* hull:hulls)b3DestroyHull(hull);}
};
}
struct SceneBodyMirror::State {
  std::shared_ptr<character_motion::MeshCollisionScene> scene;
  std::unordered_map<uint64_t,std::unique_ptr<MirrorBody>> bodies;
};
SceneBodyMirror::SceneBodyMirror():state_(std::make_unique<State>()) {}
SceneBodyMirror::~SceneBodyMirror()=default;
bool SceneBodyMirror::load(std::shared_ptr<character_motion::MeshCollisionScene> scene,const std::filesystem::path& source) {
  if(!scene || !scene->collision_world())return false;
  auto candidate=std::make_unique<State>();candidate->scene=std::move(scene);
  auto path=source;path.replace_extension(".physics.json");std::error_code ec;
  const auto size=std::filesystem::file_size(content::file_io_path(path),ec);if(ec || !size || size>16*1024*1024)return false;
  std::ifstream stream(content::file_io_path(path),std::ios::binary);std::string text(size,'\0');MirrorBodyCatalog catalog;
  constexpr glz::opts options{.error_on_unknown_keys=false};
  if(!stream.read(text.data(),std::streamsize(size)) || glz::read<options>(catalog,text) || catalog.version!=1 || catalog.bodies.size()>8192)return false;
  for(const auto& source_body:catalog.bodies) {
    if(!source_body.sourceId || candidate->bodies.contains(source_body.sourceId) || source_body.shapes.empty() || source_body.shapes.size()>256 ||
        !finite(source_body.position.data(),3) || !valid_rotation(source_body.rotation.data()))return false;
    auto body=std::make_unique<MirrorBody>();auto def=b3DefaultBodyDef();def.type=b3_kinematicBody;
    const auto& p=source_body.position;const auto& q=source_body.rotation;
    def.position={p[0],p[1],p[2]};def.rotation={{q[0],q[1],q[2]},q[3]};
    body->id=b3CreateBody(candidate->scene->collision_world()->world,&def);if(!b3Body_IsValid(body->id))return false;
    const auto shape_def=b3DefaultShapeDef();
    for(const auto& source_shape:source_body.shapes) {
      octaryn_scene_body_shape shape{};shape.kind=source_shape.kind;
      if(source_shape.points.size()%3)return false;shape.point_count=unsigned(source_shape.points.size()/3);shape.points=source_shape.points.data();
      std::copy(source_shape.localPosition.begin(),source_shape.localPosition.end(),shape.local_position);
      std::copy(source_shape.localRotation.begin(),source_shape.localRotation.end(),shape.local_rotation);
      std::copy(source_shape.halfExtents.begin(),source_shape.halfExtents.end(),shape.half_extents);
      std::copy(source_shape.capsuleA.begin(),source_shape.capsuleA.end(),shape.capsule_a);
      std::copy(source_shape.capsuleB.begin(),source_shape.capsuleB.end(),shape.capsule_b);shape.radius=source_shape.radius;
      if(!character_motion::attach_scene_body_shape(body->id,body->hulls,shape,shape_def))return false;
    }
    candidate->bodies.emplace(source_body.sourceId,std::move(body));
  }
  state_=std::move(candidate);return true;
}
bool SceneBodyMirror::pose(uint64_t source,const float* p,const float* q,const float* velocity,bool removed) {
  if(!p || !q || !velocity || !finite(p,3) || !finite(velocity,3) || !valid_rotation(q))return false;
  const auto found=state_->bodies.find(source);if(found==state_->bodies.end())return false;
  const auto body=found->second->id;
  if(removed) {if(b3Body_IsEnabled(body))b3Body_Disable(body);return true;}
  if(!b3Body_IsEnabled(body))b3Body_Enable(body);
  b3Body_SetTransform(body,{p[0],p[1],p[2]},{{q[0],q[1],q[2]},q[3]});
  b3Body_SetLinearVelocity(body,{velocity[0],velocity[1],velocity[2]});return true;
}
}
