#include "WorldLibraryRecords.h"
#include "WorldLibraryIo.h"
#include "MapModel.h"
#include "CharacterGeometry.h"
#include "MeshCollisionScene.h"
#include "MeshCollisionWorld.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace octaryn::client::app {
namespace {
struct FloorCandidate {
  float rank{};
  std::array<float,3> position{};
  bool operator<(const FloorCandidate& other) const {return rank<other.rank;}
};
bool clear_capsule(character_motion::MeshCollisionScene& scene,const std::array<float,3>& floor) {
  bool clear=true;
  using namespace character_motion;
  const b3Capsule capsule{{0,CollisionRadius,0},{0,CollisionHeight-CollisionRadius,0},CollisionRadius};
  b3World_CollideMover(scene.collision_world()->world,{floor[0],floor[1]+.025f,floor[2]},&capsule,
      b3DefaultQueryFilter(),[](b3ShapeId,const b3PlaneResult* planes,int count,void* pointer) {
        for(int index=0;index<count;++index)if(planes[index].plane.offset>.005f) {
          *static_cast<bool*>(pointer)=false;return false;
        }
        return true;
      },&clear);
  return clear;
}
bool settle_floor(character_motion::MeshCollisionScene& scene,const std::array<float,3>& floor,std::array<float,3>& spawn) {
  if(!clear_capsule(scene,floor))return false;
  character_motion::State state{};state.x=floor[0];state.y=floor[1]+character_motion::EyeOffset+.025f;state.z=floor[2];
  character_motion::Input input{};
  for(unsigned step=0;step<20;++step)character_motion::step_on_mesh(input,1.f/60,state,scene.view());
  if(!state.is_on_ground || std::abs(state.y-floor[1]-character_motion::EyeOffset)>.10f ||
      std::abs(state.x-floor[0])>.05f || std::abs(state.z-floor[2])>.05f)return false;
  spawn={state.x,state.y,state.z};return true;
}
bool publish_collision(const rendering::MapModel& model,character_motion::MeshCollisionScene& scene,uint64_t id) {
  std::vector<float> positions;positions.reserve(model.vertices.size()*3);
  for(const auto& vertex:model.vertices)positions.insert(positions.end(),vertex.position,vertex.position+3);
  character_motion::MeshCollision geometry{positions.data(),positions.size(),model.indices.data(),model.indices.size()};
  return scene.set_tile(id,geometry);
}
bool authored_spawn(const std::filesystem::path& source,const std::filesystem::path& manifest,const std::array<float,3>& authored,
    std::array<float,3>& spawn,std::string& error,const std::atomic_bool* cancel) {
  WorldAuthoredTiles tiles;if(!world_library_authored_tiles(manifest,tiles,error))return false;
  character_motion::MeshCollisionScene scene;rendering::MapLoadLimits limits;limits.cancel=cancel;
  uint64_t bytes{},loaded{};
  auto load=[&](const std::filesystem::path& path) {
    if(cancel && cancel->load()) {error="World import canceled.";return false;}
    rendering::MapModel model;
    world_library_note_io(WorldLibraryIo::ModelLoad);
    if(!rendering::load_map_model(path,model,error,limits))return false;
    bytes+=model.vertices.size()*sizeof(rendering::MapVertex)+model.indices.size()*sizeof(uint32_t);
    if(!tiles.tiles.empty() && bytes>256ull*1024*1024) {error="Authored spawn collision exceeds the import preparation budget.";return false;}
    if(!publish_collision(model,scene,++loaded)) {error="Authored spawn collision could not be prepared.";return false;}
    return true;
  };
  if(tiles.tiles.empty()) {if(!load(source))return false;}
  else for(size_t index=0;index<tiles.tiles.size();++index) {
    const auto& bounds=tiles.tiles[index];
    if(bounds[3]<authored[0]-8.5f || bounds[0]>authored[0]+8.5f || bounds[5]<authored[2]-8.5f ||
        bounds[2]>authored[2]+8.5f || bounds[4]<authored[1]-24 || bounds[1]>authored[1]+3)continue;
    if(!load(manifest.parent_path()/world_library_path(tiles.tile_files[index])))return false;
  }
  if(!loaded) {error="No authored collision tiles cover the world start position.";return false;}
  for(unsigned ring=0;ring<=16;++ring) {
    if(cancel && cancel->load()) {error="World import canceled.";return false;}
    const unsigned count=ring?8*ring:1;
    for(unsigned sample=0;sample<count;++sample) {
      const float angle=6.28318530718f*sample/count;
      const float x=authored[0]+.5f*ring*std::cos(angle),z=authored[2]+.5f*ring*std::sin(angle);
      const auto hit=b3World_CastRayClosest(scene.collision_world()->world,{x,authored[1]+1,z},{0,-24,0},b3DefaultQueryFilter());
      if(hit.hit && hit.normal.y>=.70710678f && settle_floor(scene,{x,float(hit.point.y),z},spawn))return true;
    }
  }
  error="The authored start position has no clear walkable floor nearby.";return false;
}
}
bool world_library_find_spawn(const std::filesystem::path& path,std::array<float,3>& spawn,std::string& error,
    const std::atomic_bool* cancel,const std::array<float,3>* authored,const std::filesystem::path& manifest) {
  const auto canceled=[&] {return cancel && cancel->load();};
  if(canceled()) {error="World import canceled.";return false;}
  if(authored)return authored_spawn(path,manifest,*authored,spawn,error,cancel);
  rendering::MapModel model;
  rendering::MapLoadLimits limits;limits.cancel=cancel;
  world_library_note_io(WorldLibraryIo::ModelLoad);
  if(!rendering::load_map_model(path,model,error,limits)) {error="This world could not be imported: "+error;return false;}
  float minimum[3]{},maximum[3]{};
  std::fill_n(minimum,3,std::numeric_limits<float>::max());std::fill_n(maximum,3,-std::numeric_limits<float>::max());
  std::vector<float> positions;positions.reserve(model.vertices.size()*3);
  for(const auto& vertex:model.vertices)for(unsigned axis=0;axis<3;++axis) {
    positions.push_back(vertex.position[axis]);minimum[axis]=std::min(minimum[axis],vertex.position[axis]);maximum[axis]=std::max(maximum[axis],vertex.position[axis]);
  }
  const double cx=(double(minimum[0])+maximum[0])*.5,cz=(double(minimum[2])+maximum[2])*.5;
  std::priority_queue<FloorCandidate> candidates;
  for(size_t first=0;first<model.indices.size();first+=3) {
    if((first&4095)==0 && canceled()) {error="World import canceled.";return false;}
    const auto* a=model.vertices[model.indices[first]].position;
    const auto* b=model.vertices[model.indices[first+1]].position;
    const auto* c=model.vertices[model.indices[first+2]].position;
    double ab[3],ac[3];for(unsigned axis=0;axis<3;++axis){ab[axis]=double(b[axis])-a[axis];ac[axis]=double(c[axis])-a[axis];}
    const double nx=ab[1]*ac[2]-ab[2]*ac[1],ny=ab[2]*ac[0]-ab[0]*ac[2],nz=ab[0]*ac[1]-ab[1]*ac[0];
    const double length=std::sqrt(nx*nx+ny*ny+nz*nz);
    if(!std::isfinite(length) || length<.0001 || ny/length<.70710678)continue;
    FloorCandidate candidate;
    for(unsigned axis=0;axis<3;++axis)candidate.position[axis]=float((double(a[axis])+b[axis]+c[axis])/3);
    const double dx=candidate.position[0]-cx,dz=candidate.position[2]-cz;
    // Prefer broad floors near the scene centre. Rank is smaller for better candidates.
    candidate.rank=float((dx*dx+dz*dz+1)/(length+1));
    candidates.push(candidate);if(candidates.size()>128)candidates.pop();
  }
  if(candidates.empty()) {error="This scene has no walkable floor. Export a static world with upward-facing ground.";return false;}
  std::vector<FloorCandidate> sorted;while(!candidates.empty()){sorted.push_back(candidates.top());candidates.pop();}
  std::reverse(sorted.begin(),sorted.end());
  character_motion::MeshCollisionScene scene;
  character_motion::MeshCollision geometry{positions.data(),positions.size(),model.indices.data(),model.indices.size()};
  if(canceled()) {error="World import canceled.";return false;}
  if(!scene.set_tile(1,geometry)) {error="This world could not create collision geometry.";return false;}
  for(const auto& candidate:sorted) {
    if(canceled()) {error="World import canceled.";return false;}
    if(settle_floor(scene,candidate.position,spawn))return true;
  }
  error="This scene has no clear standing position. Add a floor with room above it and try again.";return false;
}
}
