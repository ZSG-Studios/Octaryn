#pragma once
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
bool render_world_frame(WorldRenderer&,const WorldCamera&);
}
