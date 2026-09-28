#pragma once
namespace octaryn::client::rendering {
struct WorldRenderer;
struct WorldCamera;
class FrameCpuTrace;
bool render_world_frame(WorldRenderer&,const WorldCamera&,FrameCpuTrace&);
}
