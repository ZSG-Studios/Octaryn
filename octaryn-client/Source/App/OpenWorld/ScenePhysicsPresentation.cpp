#include "ScenePhysicsPresentation.h"
#include "SceneBodyMirror.h"
#include "LocalSession.h"
#include "WorldRenderer.h"
#include "GeometryTransform.h"
#include "FilePath.h"
#include <glaze/glaze.hpp>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <cmath>
#include <cstdio>

namespace octaryn::client::app {
struct BindBody {uint64_t sourceId{};std::string nodeName;std::array<float,3> position{};std::array<float,4> rotation{0,0,0,1};};
struct Catalog {unsigned version{};std::vector<BindBody> bodies;};
struct BodyPose {
  uint64_t SourceId{};float X{},Y{},Z{},RotationX{},RotationY{},RotationZ{},RotationW{1},VelocityX{},VelocityY{},VelocityZ{};unsigned Flags{};
};
struct Snapshot {std::string ScenePath;std::vector<BodyPose> Bodies;};
namespace {
constexpr auto json_options=glz::opts{.error_on_unknown_keys=false};
std::array<float,16> matrix(float x,float y,float z,float qx,float qy,float qz,float qw) {
  return {1-2*(qy*qy+qz*qz),2*(qx*qy+qz*qw),2*(qx*qz-qy*qw),0,
      2*(qx*qy-qz*qw),1-2*(qx*qx+qz*qz),2*(qy*qz+qx*qw),0,
      2*(qx*qz+qy*qw),2*(qy*qz-qx*qw),1-2*(qx*qx+qy*qy),0,x,y,z,1};
}
std::array<float,16> multiply(const std::array<float,16>& a,const std::array<float,16>& b) {
  std::array<float,16> out{};for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)
    for(unsigned k=0;k<4;++k)out[col*4+row]+=a[k*4+row]*b[col*4+k];return out;
}
}
struct ScenePhysicsPresentation::State {
  struct Bind {std::string name;std::array<float,16> inverse;};
  std::string source,last;
  std::unordered_map<uint64_t,Bind> binds;
  std::unordered_map<uint64_t,std::array<float,8>> poses;
  SceneBodyMirror mirror;
  bool mirror_ready{};
  bool initialize(const std::string& scene) {
    source=scene;binds.clear();poses.clear();last.clear();mirror_ready=false;Catalog catalog;
    auto path=std::filesystem::u8path(scene);path.replace_extension(".physics.json");
    std::error_code error;const auto size=std::filesystem::file_size(content::file_io_path(path),error);
    if(error)return true;if(size>16*1024*1024)return false;
    std::ifstream stream(content::file_io_path(path),std::ios::binary);std::string text(size,'\0');
    if(!stream.read(text.data(),std::streamsize(size)) || glz::read<json_options>(catalog,text) || catalog.version!=1)return false;
    for(const auto& body:catalog.bodies) {
      if(!body.sourceId || body.nodeName.empty() || binds.contains(body.sourceId))return false;
      const auto& p=body.position;const auto& q=body.rotation;
      rendering::virtual_geometry::GeometryTransform transform;std::string reason;
      if(!rendering::virtual_geometry::geometry_transform(matrix(p[0],p[1],p[2],q[0],q[1],q[2],q[3]),transform,reason))return false;
      std::array<float,16> inverse{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
      for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)inverse[col*4+row]=transform.inverse[row*4+col];
      binds.emplace(body.sourceId,Bind{body.nodeName,inverse});
    }
    std::printf("scene_physics_presentation mode=scene_catalog source=%s bodies=%zu authority=server\n",scene.c_str(),binds.size());
    return true;
  }
};
ScenePhysicsPresentation::ScenePhysicsPresentation():state_(std::make_unique<State>()) {}
ScenePhysicsPresentation::~ScenePhysicsPresentation()=default;
bool ScenePhysicsPresentation::update(rendering::WorldRenderer* renderer,const LocalSession& session) {
  auto& s=*state_;const auto source=rendering::open_world_renderer_scene_source(renderer);
  if(source.empty())return true;if(source!=s.source && !s.initialize(source))return false;
  if(s.binds.empty())return true;
  if(!s.mirror_ready) {
    if(!s.mirror.load(rendering::open_world_renderer_tile_collision(renderer),std::filesystem::u8path(source)))return false;
    s.mirror_ready=true;
  }
  std::string text;if(!session.scene_physics_snapshot(text) || text==s.last)return true;
  Snapshot snapshot;if(glz::read<json_options>(snapshot,text))return false;
  std::error_code error;
  if(!std::filesystem::equivalent(content::file_io_path(std::filesystem::u8path(snapshot.ScenePath)),
      content::file_io_path(std::filesystem::u8path(source)),error) || error)return true;
  std::vector<rendering::SceneObjectPose> objects;objects.reserve(snapshot.Bodies.size());
  std::vector<std::pair<uint64_t,std::array<float,8>>> changes;
  for(const auto& pose:snapshot.Bodies) {
    const auto bind=s.binds.find(pose.SourceId);if(bind==s.binds.end())continue;
    const float norm=pose.RotationX*pose.RotationX+pose.RotationY*pose.RotationY+pose.RotationZ*pose.RotationZ+pose.RotationW*pose.RotationW;
    if(!std::isfinite(norm) || std::abs(norm-1)>0.01f)return false;
    const std::array<float,8> key{pose.X,pose.Y,pose.Z,pose.RotationX,pose.RotationY,pose.RotationZ,pose.RotationW,float((pose.Flags&4)!=0)};
    const float velocity[]={pose.VelocityX,pose.VelocityY,pose.VelocityZ};
    if(!s.mirror.pose(pose.SourceId,key.data(),key.data()+3,velocity,(pose.Flags&(4|8))!=0))return false;
    if(const auto old=s.poses.find(pose.SourceId);old!=s.poses.end() && old->second==key)continue;
    const auto current=matrix(pose.X,pose.Y,pose.Z,pose.RotationX,pose.RotationY,pose.RotationZ,pose.RotationW);
    objects.push_back({bind->second.name,multiply(current,bind->second.inverse),(pose.Flags&4)!=0});
    changes.push_back({pose.SourceId,key});
  }
  if(!objects.empty() && !rendering::open_world_renderer_set_scene_objects(renderer,objects))return false;
  for(const auto& [id,key]:changes)s.poses[id]=key;
  s.last=std::move(text);return true;
}
}
