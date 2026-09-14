#pragma once
namespace rhi {class ICommandEncoder;}
namespace octaryn::client::rendering {
struct WorldRenderer;
bool initialize_lighting(WorldRenderer&);
bool render_lighting(WorldRenderer&,rhi::ICommandEncoder*);
}
