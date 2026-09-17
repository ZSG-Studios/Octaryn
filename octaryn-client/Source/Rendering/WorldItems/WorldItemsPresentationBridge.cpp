#include "WorldItemsPresentationBridge.h"
#include "WorldRendererInternal.h"
#include "WorldItemsClient.h"

namespace octaryn::client::rendering {
void set_world_items_presentation(WorldRenderer* renderer,
 const world_presentation::WorldItemPresentation& presentation) {
 if(!renderer)return;
 renderer->item_snapshot=presentation.authoritative;
 set_provisional_toss(renderer->items,presentation.provisional);
}
}
