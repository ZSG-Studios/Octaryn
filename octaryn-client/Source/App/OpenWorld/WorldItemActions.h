#pragma once
#include "GameUi.h"
#include "WorldItemsClient.h"

namespace octaryn::client::app {
class WorldItemActions {
  GameUiDropRequest pending_{};
public:
  void update(GameUi& ui,world_presentation::WorldItemsClient& items) {
    // A recovered transport receipt is processed before a restored UI outbox.
    world_presentation::DropReceipt receipt;
    if(items.drop_receipt(receipt) && ui.finish_drop(receipt.accepted(),receipt.command_id)) {
      items.acknowledge_drop(receipt.command_id);pending_={};
    }
    if(!pending_.count)ui.take_drop_request(pending_);
    if(pending_.count && items.submit_drop(pending_.block_id,pending_.count))pending_={};
    world_presentation::PickupGrant grant;
    if(items.next_pickup(grant) && ui.apply_pickup(grant.id,static_cast<std::uint16_t>(grant.block),grant.count))
      items.acknowledge_pickup(grant.id);
  }
};
}
