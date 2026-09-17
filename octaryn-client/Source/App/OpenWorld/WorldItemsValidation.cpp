#include "WorldItemsValidation.h"
#include "WorldItemsClient.h"
#include "GameUi.h"
#include "WorldRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace octaryn::client::app {
WorldItemsValidation::~WorldItemsValidation() {
 if(!held_intent_.empty()) {std::error_code error;std::filesystem::remove(held_intent_,error);}
}
void WorldItemsValidation::observe(GameUi& ui,world_presentation::WorldItemsClient& items,
    const LocalPlayerPose& pose,bool ready,double seconds) {
 if(started_<0) {
 started_=seconds;
 if(const char* value=SDL_getenv("OCTARYN_CLIENT_VALIDATE_PROVISIONAL_TOSS"))
  qualify_provisional_=std::strcmp(value,"1")==0;
 if(qualify_provisional_) {
  const char* world=SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
  const char* capture=SDL_getenv("OCTARYN_CLIENT_CAPTURE_PATH");
  if(!world||!*world||!capture||!*capture)
   throw std::runtime_error("Provisional qualification requires isolated WORLD_PATH and CAPTURE_PATH");
  accepted_capture_=std::filesystem::path(reinterpret_cast<const char8_t*>(capture));
  provisional_capture_=accepted_capture_;provisional_capture_+=".provisional.bmp";
  auto accepted_sample=accepted_capture_;accepted_sample+=".sample-1.bmp";
  if(std::filesystem::exists(accepted_capture_)||std::filesystem::exists(provisional_capture_)||
   std::filesystem::exists(accepted_sample))
   throw std::runtime_error("Provisional qualification requires fresh capture paths");
  auto* env=SDL_GetEnvironment();
  SDL_SetEnvironmentVariable(env,"OCTARYN_CLIENT_CAPTURE_COUNT","2",true);
  SDL_SetEnvironmentVariable(env,"OCTARYN_CLIENT_CAPTURE_STRIDE","1",true);
  SDL_SetEnvironmentVariable(env,"OCTARYN_CLIENT_CAPTURE_MIN_FRAME","120",true);
  const auto path=provisional_capture_.u8string();
  SDL_SetEnvironmentVariable(env,"OCTARYN_CLIENT_CAPTURE_PATH",reinterpret_cast<const char*>(path.c_str()),true);
 }
    if(const char* count=SDL_getenv("OCTARYN_CLIENT_VALIDATE_ITEM_COUNT")) {
      if(!std::strcmp(count,"999"))requested_count_=999;
      else if(std::strcmp(count,"1"))throw std::runtime_error("Item qualification count must be1 or999");
    }
    expected_entities_=(requested_count_+63)/64;
  }
  if(seconds-started_>90)throw std::runtime_error("World item qualification exceeded90 seconds");
 if(phase_==Phase::Start) {
 if(qualify_provisional_&&rendered_frames_<120)return;
    if(!ready||items.status()!="world_items_ready")return;
    if(std::abs(pose.yaw)>.05f||std::abs(pose.pitch+.8f)>.05f)return;
    if(!items.snapshot()->items.empty())throw std::runtime_error("World item qualification requires an isolated empty item world");
    block_=ui.selected_block();original_count_=ui.inventory_count(block_);
    original_drop_=ui.inventory_drop_watermark();original_grant_=ui.inventory_grant_watermark();
 last_grant_=original_grant_;
 if(qualify_provisional_) {
  const auto root=std::filesystem::path(reinterpret_cast<const char8_t*>(SDL_getenv("OCTARYN_CLIENT_WORLD_PATH")));
  if(std::filesystem::exists(root/"runtime/world_items.intent"))
   throw std::runtime_error("Provisional qualification requires a fresh local intent path");
  const auto hold=root/"runtime/world_items.intent.tmp";
  // Block only the publisher's temporary file. The server still sees no intent.
  // The durable outbox is written first and remains available for retry.
  if(!std::filesystem::create_directory(hold))
   throw std::runtime_error("Provisional qualification requires an unused local intent path");
  held_intent_=hold;
 }
    if(!block_||original_count_<requested_count_||!ui.validation_request_item_drop(requested_count_>1))
      throw std::runtime_error("World item qualification could not reserve the requested inventory count");
    phase_=Phase::Drop;phase_started_=seconds;
    std::printf("world_items_validation phase=toss_requested block=%u count=%u inventory_before=%u\n",block_,requested_count_,original_count_);
    return;
 }
 if(qualify_provisional_&&!provisional_captured_&&phase_!=Phase::Start) {
  if(seconds-phase_started_>5)throw std::runtime_error("Provisional capture did not complete within five seconds");
  const auto visual=items.presentation();
  if(!visual.authoritative->items.empty()||ui.inventory_drop_watermark()!=original_drop_||
   ui.inventory_grant_watermark()!=original_grant_)
   throw std::runtime_error("Authority advanced while provisional intent was held");
  if(visual.provisional) {
   const auto& p=*visual.provisional;
   if(p.block!=block_||p.count!=requested_count_)
    throw std::runtime_error("Provisional presentation lost reserved material/count");
   target_.x=p.x;target_.y=p.y;target_.z=p.z;seen_item_=true;
   if(p.age<.15)return;
   if(phase_!=Phase::Provisional) {
    provisional_request_=p.request;provisional_command_=p.command;
    std::printf("world_items_validation phase=provisional request=%llu command=%llu authoritative=0 provisional=1 count=%u position=%.5f,%.5f,%.5f\n",
     static_cast<unsigned long long>(p.request),static_cast<unsigned long long>(p.command),p.count,p.x,p.y,p.z);
    std::fflush(stdout);phase_=Phase::Provisional;
   }
  } else if(phase_==Phase::Provisional)
   throw std::runtime_error("Provisional toss expired before actual GPU capture");
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
 if(qualify_provisional_) {
  const auto visual=items.presentation();
  if(visual.provisional)throw std::runtime_error("Accepted receipt retained provisional toss");
  std::printf("world_items_validation phase=provisional_reconciled request=%llu command=%llu authoritative=%zu provisional=0\n",
   static_cast<unsigned long long>(provisional_request_),static_cast<unsigned long long>(provisional_command_),
   visual.authoritative->items.size());
 }
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
 ++rendered_frames_;
 if(phase_==Phase::Provisional) {
  if(!captured||!std::filesystem::exists(provisional_capture_))return;
  std::printf("world_items_validation phase=provisional_captured request=%llu command=%llu authoritative=0 provisional=1 path=%s\n",
   static_cast<unsigned long long>(provisional_request_),static_cast<unsigned long long>(provisional_command_),
   provisional_capture_.string().c_str());std::fflush(stdout);
  const auto path=accepted_capture_.u8string();
  SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"OCTARYN_CLIENT_CAPTURE_PATH",reinterpret_cast<const char*>(path.c_str()),true);
  std::filesystem::remove(held_intent_);held_intent_.clear();
  provisional_captured_=true;seen_item_=false;target_={};phase_=Phase::Drop;
  return;
 }
 if(phase_!=Phase::Visible||!captured)return;
 if(qualify_provisional_) {
  // The existing renderer numbers its second readback; retain the original accepted BMP path too.
  auto sample=accepted_capture_;sample+=".sample-1.bmp";
  if(!std::filesystem::exists(sample))return;
  std::filesystem::copy_file(sample,accepted_capture_,std::filesystem::copy_options::none);
  std::printf("world_items_validation phase=authoritative_captured authoritative=%u provisional=0 path=%s source=%s\n",
   expected_entities_,accepted_capture_.string().c_str(),sample.string().c_str());
 }
  phase_=Phase::Pickup;
  std::printf("world_items_validation phase=captured item=%llu position=%.5f,%.5f,%.5f count=%u\n",
    static_cast<unsigned long long>(first_item_),target_.x,target_.y,target_.z,requested_count_);
  std::fflush(stdout);
}
}
