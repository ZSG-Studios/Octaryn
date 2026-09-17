#include "BlockActionsValidation.h"
#include "LocalSession.h"
#include "WorldRenderer.h"
#include "WorldStream.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace octaryn::client::app {
namespace wp=world_presentation;
BlockActionsValidation::~BlockActionsValidation() {
 if(!hold_.empty()){std::error_code error;std::filesystem::remove(hold_,error);}
}
const char* BlockActionsValidation::phase() const {
 static const char* names[]{"baseline","pending_reject","rollback","pending_accept",
  "accepted_break","pending_restore","accepted_restore","complete"};
 return names[stage_];
}
std::filesystem::path BlockActionsValidation::sample() const {
 auto path=capture_;if(stage_)path+=".sample-"+std::to_string(stage_)+".bmp";return path;
}
void BlockActionsValidation::input(LocalPlayerInput& input) const {
 input={};input.yaw=0;input.pitch=-.8f;
}
void BlockActionsValidation::camera(rendering::WorldCamera& camera) const {
 camera.yaw=0;camera.pitch=-.8f;
 if(!selected_)return;
 const float dx=static_cast<float>(cell_.x)+.5f-camera.x;
 const float dy=static_cast<float>(cell_.y)+.5f-camera.y,dz=static_cast<float>(cell_.z)+.5f-camera.z;
 camera.yaw=std::atan2(dx,-dz);camera.pitch=std::atan2(dy,std::sqrt(dx*dx+dz*dz));
}
void BlockActionsValidation::update(wp::WorldStream& stream,wp::BlockInteraction& interaction,
 const LocalPlayerPose& pose,bool ready,double seconds,const Submit& submit) {
 capture_ready_=false;if(complete())return;
 if(started_<0)started_=seconds;
 if(seconds-started_>120)throw std::runtime_error("Block action GPU qualification timed out");
 if(!initialized_) {
  const auto* path=SDL_getenv("OCTARYN_CLIENT_CAPTURE_PATH");
  if(!path||!*path)throw std::runtime_error("Block qualification requires capture path");
  capture_=std::filesystem::path(reinterpret_cast<const char8_t*>(path));
  for(unsigned i=0;i<7;++i) {
   auto check=capture_;if(i)check+=".sample-"+std::to_string(i)+".bmp";
   if(std::filesystem::exists(check))throw std::runtime_error("Block captures must be fresh");
  }
  auto* env=SDL_GetEnvironment();
  SDL_SetEnvironmentVariable(env,"OCTARYN_CLIENT_CAPTURE_COUNT","7",true);
  SDL_SetEnvironmentVariable(env,"OCTARYN_CLIENT_CAPTURE_STRIDE","1",true);
  SDL_SetEnvironmentVariable(env,"OCTARYN_CLIENT_CAPTURE_MIN_FRAME","120",true);
  initialized_=true;
 }
 if(!selected_) {
  if(!ready||frames_<120||!pose.on_ground)return;
  const auto& target=interaction.target();
  std::uint16_t below{};
  if(!target.hit||!target.actionable||!target.block_id||
   !stream.try_block(target.block.x,target.block.y-1,target.block.z,below)||!below)return;
  cell_=target.block;base_=target.block_id;selected_=true;
  std::printf("block_actions_validation phase=selected cell=%d,%d,%d base=%u\n",cell_.x,cell_.y,cell_.z,base_);
 }
 if((stage_&1)&&!submitted_) {
  if(!ready)return;
  const auto* root=SDL_getenv("OCTARYN_CLIENT_WORLD_PATH");
  const auto path=std::filesystem::path(reinterpret_cast<const char8_t*>(root))/"runtime/block_interaction.json.tmp";
  if(!std::filesystem::create_directory(path))throw std::runtime_error("Block qualification intent barrier already exists");
  hold_=path;
  wp::BlockEditIntent edit{cell_,cell_,static_cast<std::uint16_t>(stage_==5?base_:0),pose.x,pose.y,pose.z};
  if(stage_==1)edit.hit.x+=100; // Deliberately invalid metadata, same predicted visible cell.
  if(stage_==5)--edit.hit.y;
  if(!submit(edit,&command_))throw std::runtime_error("Block qualification could not queue production edit");
  submitted_=true;last_submit_=seconds;
  std::printf("block_actions_validation phase=%s command=%llu cell=%d,%d,%d base=%u predicted=%u receipt=pending\n",
   phase(),static_cast<unsigned long long>(command_),cell_.x,cell_.y,cell_.z,base_,edit.block);
 }
 std::uint16_t actual{};
 const auto expected=(stage_==1||stage_==3||stage_==4)?0:base_;
 if(!stream.try_block(cell_.x,cell_.y,cell_.z,actual)||actual!=expected)
  throw std::runtime_error("Block reconciliation flicker/query material mismatch");
 if(waiting_receipt_) {
  if(seconds-last_submit_>20)throw std::runtime_error("Block qualification receipt timeout");
  return;
 }
 if(!ready){stable_frames_=0;stable_since_=-1;return;}
 if(stable_since_<0)stable_since_=seconds;
 ++stable_frames_;
 capture_ready_=(stage_&1)||(stable_frames_>=120&&seconds-stable_since_>=2);
}
void BlockActionsValidation::receipt(const BlockReceipt& receipt) {
 if(!submitted_||receipt.commandID!=command_)return;
 if(!waiting_receipt_||!hold_.empty())throw std::runtime_error("Block authority bypassed qualification hold");
 const bool accepted=stage_!=1;
 const auto expected=stage_==3?0:base_;
 bool found=false;
 for(const auto& cell:receipt.blocks)if(cell.x==cell_.x&&cell.y==cell_.y&&cell.z==cell_.z) {
  found=true;if(cell.block!=expected)throw std::runtime_error("Block receipt authoritative material mismatch");
 }
 if(receipt.accepted!=accepted||!found)throw std::runtime_error("Unexpected block acceptance or missing receipt cell");
 std::printf("block_actions_validation phase=receipt command=%llu accepted=%u revision=%llu base=%u material=%u\n",
  static_cast<unsigned long long>(command_),receipt.accepted,static_cast<unsigned long long>(receipt.revision),base_,expected);
 ++stage_;submitted_=false;waiting_receipt_=false;stable_frames_=0;stable_since_=-1;
}
void BlockActionsValidation::frame_rendered() {
 ++frames_;if(!capture_ready_||!std::filesystem::exists(sample()))return;
 const unsigned material=(stage_==1||stage_==3||stage_==4)?0:base_;
 std::printf("block_actions_validation phase=captured state=%s command=%llu base=%u material=%u pending_meshes=0 path=%s\n",
  phase(),static_cast<unsigned long long>(command_),base_,material,sample().string().c_str());std::fflush(stdout);
 capture_ready_=false;stable_frames_=0;stable_since_=-1;
 if(stage_&1) {
  if(!std::filesystem::remove(hold_))throw std::runtime_error("Block intent barrier release failed");
  hold_.clear();waiting_receipt_=true;
 } else ++stage_;
 if(complete()){std::puts("block_actions_validation=passed captures=7 os_events_injected=0");std::fflush(stdout);}
}
}
