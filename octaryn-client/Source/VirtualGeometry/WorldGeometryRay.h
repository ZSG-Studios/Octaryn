#pragma once
#include "RayGeometry.h"
#include <memory>
namespace octaryn::client::rendering {
struct WorldRenderer;
struct MapRenderer;
struct WorldCamera;
namespace virtual_geometry {
struct MapGeometryCache;
std::uint64_t geometry_ray_reservation(const MapGeometryCache&);
std::uint64_t geometry_ray_instance_reservation(const MapGeometryCache&,std::uint64_t nodes);
class WorldGeometryRay {
public:
  WorldGeometryRay();
  ~WorldGeometryRay();
  bool prepare(WorldRenderer&,MapRenderer&,const WorldCamera&,bool notify_scene=true);
  std::shared_ptr<const RaySnapshot> snapshot() const;
  const std::string& error() const;
  std::uint64_t gpu_bytes() const;
  bool idle() const;
  bool memory_blocked() const;
private:
  struct State;
  std::unique_ptr<State> state_;
};
bool prepare_geometry_ray(WorldRenderer&,MapRenderer&,const WorldCamera&,bool notify_scene=true);
}
}
