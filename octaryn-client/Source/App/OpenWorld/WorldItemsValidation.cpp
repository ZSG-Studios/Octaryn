#include "WorldItemsValidation.h"
#include "WorldItemsClient.h"
#include "GameUi.h"
#include "WorldRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace octaryn::client::app {
void WorldItemsValidation::observe(GameUi& ui,world_presentation::WorldItemsClient& items,
    const LocalPlayerPose& pose,bool ready,double seconds) {
  if(started_<0) {
    started_=seconds;
    if(const char* count=std::getenv("OCTARYN_CLIENT_VALIDATE_ITEM_COUNT")) {
      if(!std::strcmp(count,"999"))requested_count_=999;
      else if(std::strcmp(count,"1"))throw std::runtime_error("Item qualification count must be1 or999");
    }
    expected_entities_=(requested_count_+63)/64;
  }
  if(seconds-started_>90)throw std::runtime_error("World item qualification exceeded90 seconds");
  if(phase_==Phase::Start) {
    if(!ready||items.status()!="world_items_ready")return;
    if(std::abs(pose.yaw)>.05f||std::abs(pose.pitch+.8f)>.05f)return;
    if(!items.snapshot()->items.empty())throw std::runtime_error("World item qualification requires an isolated empty item world");
    block_=ui.selected_block();original_count_=ui.inventory_count(block_);
    original_drop_=ui.inventory_drop_watermark();original_grant_=ui.inventory_grant_watermark();
    last_grant_=original_grant_;
    if(!block_||original_count_<requested_count_||!ui.validation_request_item_drop(requested_count_>1))
      throw std::runtime_error("World item qualification could not reserve the requested inventory count");
    phase_=Phase::Drop;phase_started_=seconds;
    std::printf("world_items_validation phase=toss_requested block=%u count=%u inventory_before=%u\n",block_,requested_count_,original_count_);
    return;
  }
  const auto snapshot=items.snapshot();
  const auto found=std::find_if(snapshot->items.begin(),snapshot->items.end(),[&](const auto& item){
    return seen_item_?item.id==target_.id:item.block==block_&&item.count>0;
  });
  bool present=found!=snapshot->items.end();
  if(present){target_=*found;seen_item_=true;if(!first_item_)first_item_=target_.id;}
  if(phase_==Phase::Pickup) {
    const auto nearest=std::min_element(snapshot->items.begin(),snapshot->items.end(),[&](const auto& a,const auto& b){
      const auto distance=[&](const auto& item){return (item.x-pose.x)*(item.x-pose.x)+(item.y+1.5f-pose.y)*(item.y+1.5f-pose.y)+(item.z-pose.z)*(item.z-pose.z);};
      return distance(a)<distance(b);
    });
    present=nearest!=snapshot->items.end();
    if(present) {
      if(nearest->block!=block_)throw std::runtime_error("Unexpected item in isolated pickup qualification");
      target_=*nearest;
    }
    // Observe the saved UI receipt on the app thread; worker publication may race actions.update().
    const auto grant=ui.inventory_grant_watermark();
    const auto inventory_count=ui.inventory_count(block_);
    if(grant>last_grant_) {
      if(grant!=last_grant_+1||inventory_count<=credited_inventory_||inventory_count-credited_inventory_>64)
        throw std::runtime_error("Pickup grant ordering or stack bounds violated");
      const auto count=inventory_count-credited_inventory_;
      last_grant_=grant;credited_inventory_=inventory_count;++grant_count_;grant_total_+=count;
      if(grant_total_>requested_count_||grant_count_>expected_entities_)
        throw std::runtime_error("Pickup grants exceed the authoritative tossed count");
      std::printf("world_items_validation phase=pickup_credit grant=%llu block=%u count=%u\n",
          static_cast<unsigned long long>(grant),block_,count);
    }
  }
  if(phase_==Phase::Drop) {
    if(ui.inventory_drop_watermark()>original_drop_&&ui.inventory_count(block_)==original_count_)
      throw std::runtime_error("Authoritative world item toss was rejected");
    if(present&&ui.inventory_count(block_)==original_count_-requested_count_&&ui.inventory_drop_watermark()>original_drop_) {
      std::uint32_t total{},full{},remainder{};
      std::uint64_t last{};
      for(const auto& item:snapshot->items) {
        if(item.block!=block_||!item.count||item.count>64||item.id<=last)
          throw std::runtime_error("Authoritative split entity identity/stack invalid");
        total+=item.count;full+=item.count==64?1u:0u;if(item.count<64)remainder+=item.count;last=item.id;
      }
      if(snapshot->items.size()!=expected_entities_||total!=requested_count_||
          full!=requested_count_/64||remainder!=requested_count_%64)
        throw std::runtime_error("Authoritative toss did not publish the complete bounded stack split");
      phase_=Phase::Visible;phase_started_=seconds;
      credited_inventory_=ui.inventory_count(block_);
      std::printf("world_items_validation phase=accepted item=%llu block=%u count=%u inventory_after_drop=%u server_seconds=%.6f\n",
        static_cast<unsigned long long>(first_item_),block_,requested_count_,ui.inventory_count(block_),snapshot->source_seconds);
      std::printf("world_items_validation phase=split entities=%u total=%u full_stacks=%u remainder=%u first_item=%llu last_item=%llu\n",
          expected_entities_,total,full,remainder,static_cast<unsigned long long>(first_item_),static_cast<unsigned long long>(last));
    }
  } else if(phase_==Phase::Visible&&!present) {
    throw std::runtime_error("World item disappeared before its real GPU capture");
  } else if(phase_==Phase::Pickup&&ready&&!present&&ui.inventory_count(block_)==original_count_&&
      ui.inventory_grant_watermark()>original_grant_&&items.acknowledged_pickup()>=ui.inventory_grant_watermark()) {
    if(grant_count_!=expected_entities_||grant_total_!=requested_count_)
      throw std::runtime_error("Completed pickup lacks conserved ordered grant evidence");
    phase_=Phase::Complete;
    std::printf("world_items_validation=passed item=%llu block=%u count=%u inventory_before=%u inventory_after=%u grant=%llu server_ack=%llu capture=completed os_events_injected=0\n",
      static_cast<unsigned long long>(first_item_),block_,requested_count_,original_count_,ui.inventory_count(block_),
      static_cast<unsigned long long>(ui.inventory_grant_watermark()),static_cast<unsigned long long>(items.acknowledged_pickup()));
    std::fflush(stdout);
  }
  if(phase_!=Phase::Complete&&seconds-phase_started_>(phase_==Phase::Pickup?40:20))
    throw std::runtime_error("World item qualification phase stalled");
}
void WorldItemsValidation::input(LocalPlayerInput& input,const LocalPlayerPose& pose) const {
  input={};input.yaw=0;input.pitch=-.8f;
  if(phase_!=Phase::Pickup)return;
  const float dx=target_.x-pose.x,dz=target_.z-pose.z;
  const float horizontal=std::sqrt(dx*dx+dz*dz);
  input.yaw=std::atan2(dx,-dz);input.flying=true;
  input.forward=horizontal>.3f;
  const auto vertical=target_.y+1.5f-pose.y;
  input.up=vertical>.25f;input.down=vertical<-.25f;
}
void WorldItemsValidation::camera(rendering::WorldCamera& camera) const {
  if(!seen_item_)return;
  const float dx=target_.x-camera.x,dy=target_.y+.1f-camera.y,dz=target_.z-camera.z;
  camera.yaw=std::atan2(dx,-dz);camera.pitch=std::atan2(dy,std::sqrt(dx*dx+dz*dz));
}
void WorldItemsValidation::frame_rendered(bool captured) {
  if(phase_!=Phase::Visible||!captured)return;
  phase_=Phase::Pickup;
  std::printf("world_items_validation phase=captured item=%llu position=%.5f,%.5f,%.5f count=%u\n",
    static_cast<unsigned long long>(first_item_),target_.x,target_.y,target_.z,requested_count_);
  std::fflush(stdout);
}
}
