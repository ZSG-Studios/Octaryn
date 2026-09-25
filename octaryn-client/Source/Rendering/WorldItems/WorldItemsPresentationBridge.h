#pragma once
namespace octaryn::client::world_presentation {struct WorldItemPresentation;}
namespace octaryn::client::rendering {
struct WorldRenderer;
// Call on the render owner thread after actions, before submitting the frame.
void set_world_items_presentation(WorldRenderer*,const world_presentation::WorldItemPresentation&);
}
