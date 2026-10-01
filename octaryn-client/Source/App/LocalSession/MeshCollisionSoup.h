#pragma once

#include "CharacterMotion.h"
#include "MeshCollisionScene.h"
#include <memory>
#include <functional>
#include <utility>
#include <vector>

namespace octaryn::client::app::local_session {

// Immutable geometry owns its cached world. Prediction retains the same asset
// while a loading worker prepares the replacement map.
class MeshCollisionSoup {
  struct Geometry {
    std::vector<float> positions;
    std::vector<std::uint32_t> indices;
    Geometry(std::vector<float> points, std::vector<std::uint32_t> triangles)
        : positions(std::move(points)), indices(std::move(triangles)) {}
    character_motion::MeshCollision view() const {
      return {positions.data(), positions.size(), indices.data(), indices.size()};
    }
    ~Geometry() { character_motion::release_mesh_collision(view()); }
  };
  std::shared_ptr<const Geometry> geometry_;
  std::shared_ptr<character_motion::MeshCollisionScene> scene_;
  std::function<bool(float,float,float,float)> ready_;

public:
  MeshCollisionSoup() = default;
  explicit MeshCollisionSoup(std::shared_ptr<character_motion::MeshCollisionScene> scene,
      std::function<bool(float,float,float,float)> ready={})
      : scene_(std::move(scene)),ready_(std::move(ready)) {}
  bool ready(float x,float y,float z,float radius=3) const {return !ready_ || ready_(x,y,z,radius);}
  MeshCollisionSoup(std::vector<float> positions, std::vector<std::uint32_t> indices)
      : geometry_(std::make_shared<Geometry>(std::move(positions), std::move(indices))) {}
  character_motion::MeshCollision view() const {
    if (scene_) return scene_->view();
    return geometry_ ? geometry_->view() : character_motion::MeshCollision{};
  }
};

}
