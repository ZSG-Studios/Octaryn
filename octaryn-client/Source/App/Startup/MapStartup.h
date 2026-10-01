#pragma once
#include <functional>
#include <filesystem>
#include <string>

struct SDL_Window;
namespace octaryn::client::rendering {struct WorldRenderer;}
namespace octaryn::client::app {
class LocalSession;
struct MapManifest;
namespace local_session {struct MeshCollisionSoup;}
}

namespace octaryn::client::app {
// Returns after map loading has stopped using the renderer, including on close.
// Fills collision_out with the map soup and warms the Box3D collision world
// for client-side prediction; collision_out must outlive the session that
// consumes it.
bool start_map(SDL_Window* window, rendering::WorldRenderer* renderer,
    const MapManifest& manifest, bool& running, local_session::MeshCollisionSoup& collision_out,
    std::function<bool(const std::string&,bool draw)> present={});
struct MapStartupOutcome {bool ready{},cancelled{};std::string error;};
MapStartupOutcome start_world_map(SDL_Window*,rendering::WorldRenderer*,const std::filesystem::path& world,
    const std::filesystem::path& bundle,MapManifest& manifest,bool& running,
    local_session::MeshCollisionSoup&,std::function<bool(const std::string&,bool draw)> present);
}
