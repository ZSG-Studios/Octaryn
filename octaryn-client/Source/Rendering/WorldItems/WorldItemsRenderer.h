#pragma once
#include <slang-rhi.h>
#include "PlayerPose.h"
namespace octaryn::client::world_presentation {struct WorldItemSnapshot;}
namespace octaryn::client::rendering {
struct WorldCamera;struct WorldAtlas;struct WorldItemsRenderer;
WorldItemsRenderer* create_world_items_renderer(rhi::IDevice*,rhi::Format color,
    rhi::Format depth,const char* shader_path);
void destroy_world_items_renderer(WorldItemsRenderer*);
bool render_world_items(WorldItemsRenderer*,rhi::IRenderPassEncoder*,const WorldCamera&,
    int width,int height,WorldAtlas*,const world_presentation::WorldItemSnapshot&,
    double elapsed_seconds,const PlayerLighting&,bool temporal=false,bool reset=false,float mip_bias=0);
void commit_world_items_frame(WorldItemsRenderer*);
}
