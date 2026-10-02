#pragma once
#include "ModuleHost.h"
namespace octaryn::client::app {
struct WorldSession;
// CPU activation runs on one scheduler worker; every presentation API crosses
// StartupWork back to the owning thread while loading continues to draw.
int start_world_module(WorldSession&,const host::ModuleHostHooks&);
}
