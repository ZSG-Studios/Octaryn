#pragma once
#include <slang-rhi.h>
namespace octaryn::client::rendering {
struct WorldRenderer;
bool world_ddgi_debug_initialize(WorldRenderer&);
bool world_ddgi_debug(WorldRenderer&,rhi::ICommandEncoder*);
}
