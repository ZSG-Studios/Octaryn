#pragma once

#include "MapManifest.h"
#include "MapSceneGeometry.h"

namespace octaryn::server::map_world {

// Loaded map world: one static collision soup plus the manifest spawn pose.
struct ServerMapWorld {
  MapTriangleSoup soup;
  MapManifest manifest;
};

} // namespace octaryn::server::map_world
