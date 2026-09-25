#pragma once
#include <slang-rhi.h>
#include "PlayerPose.h"
#include "ProvisionalToss.h"
#include <optional>
namespace octaryn::client::world_presentation {struct WorldItemSnapshot;}
namespace octaryn::client::rendering {
struct WorldRenderer;struct WorldCamera;struct WorldAtlas;struct WorldItemsRenderer;
struct DynamicReceivers;
WorldItemsRenderer* create_world_items_renderer(rhi::IDevice*,rhi::Format color,
    rhi::Format depth,const char* shader_path);
void destroy_world_items_renderer(WorldItemsRenderer*);
void set_provisional_toss(WorldItemsRenderer*,const std::optional<world_presentation::ProvisionalToss>&);
bool prepare_world_items_frame(WorldItemsRenderer*,WorldRenderer&,rhi::ICommandEncoder*,const WorldCamera&,
    int width,int height,WorldAtlas*,const world_presentation::WorldItemSnapshot&,
    double elapsed_seconds,const PlayerLighting&,bool reset=false,float mip_bias=0);
bool render_world_items(WorldItemsRenderer*,rhi::IRenderPassEncoder*,WorldAtlas*,WorldRenderer&,bool temporal=false);
void commit_world_items_frame(WorldItemsRenderer*);
bool initialize_item_receivers(WorldItemsRenderer*,WorldRenderer&);
const DynamicReceivers* item_receiver_stats(const WorldItemsRenderer*);
}
