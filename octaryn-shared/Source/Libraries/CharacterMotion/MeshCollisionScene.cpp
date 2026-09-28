#include "MeshCollisionScene.h"
#include "MeshCollisionWorld.h"
#include <unordered_map>

namespace octaryn::character_motion {
struct MeshCollisionScene::State {
  struct Tile {
    std::unique_ptr<PreparedCollisionTile> mesh;
    b3BodyId body{};
  };
  std::unordered_map<uint64_t, Tile> tiles;
  // Destroy the world and shapes before releasing any retained mesh data.
  MeshCollisionWorld collision;
};

MeshCollisionScene::MeshCollisionScene() : state_(std::make_unique<State>()) {
  auto definition = b3DefaultWorldDef();
  definition.workerCount = 1;
  state_->collision.world = b3CreateWorld(&definition);
}
MeshCollisionScene::~MeshCollisionScene() = default;
PreparedCollisionTile::~PreparedCollisionTile() {
  if (mesh_) b3DestroyMesh(static_cast<b3MeshData*>(mesh_));
}

std::unique_ptr<PreparedCollisionTile> MeshCollisionScene::prepare_tile(const MeshCollision& source) {
  bool valid_empty=false;
  auto* mesh = build_collision_mesh(source,&valid_empty);
  return mesh || valid_empty ? std::unique_ptr<PreparedCollisionTile>(new PreparedCollisionTile(mesh)) : nullptr;
}

bool MeshCollisionScene::set_tile(uint64_t id, const MeshCollision& source) {
  return set_tile(id, prepare_tile(source));
}
bool MeshCollisionScene::set_tile(uint64_t id, std::unique_ptr<PreparedCollisionTile> prepared) {
  if (!b3World_IsValid(state_->collision.world)) return false;
  State::Tile tile;
  tile.mesh = std::move(prepared);
  if (!tile.mesh) return false;
  if (!tile.mesh->mesh_) {
    remove_tile(id);
    state_->tiles.emplace(id,std::move(tile));
    return true;
  }
  auto bodyDefinition = b3DefaultBodyDef();
  tile.body = b3CreateBody(state_->collision.world, &bodyDefinition);
  if (!b3Body_IsValid(tile.body)) return false;
  auto shapeDefinition = b3DefaultShapeDef();
  const auto shape = b3CreateMeshShape(tile.body, &shapeDefinition,
                                    static_cast<b3MeshData*>(tile.mesh->mesh_), b3Vec3_one);
  if (!b3Shape_IsValid(shape)) {
    b3DestroyBody(tile.body);
    return false;
  }
  remove_tile(id);
  state_->tiles.emplace(id, std::move(tile));
  return true;
}

void MeshCollisionScene::remove_tile(uint64_t id) {
  const auto found = state_->tiles.find(id);
  if (found == state_->tiles.end()) return;
  if (b3Body_IsValid(found->second.body))b3DestroyBody(found->second.body);
  state_->tiles.erase(found);
}
bool MeshCollisionScene::contains(uint64_t id) const { return state_->tiles.contains(id); }
MeshCollision MeshCollisionScene::view() { return {nullptr, 0, nullptr, 0, this}; }
MeshCollisionWorld* MeshCollisionScene::collision_world() {
  return b3World_IsValid(state_->collision.world) ? &state_->collision : nullptr;
}
}
