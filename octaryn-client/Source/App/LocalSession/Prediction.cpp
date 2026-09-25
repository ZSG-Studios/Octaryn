#include "Prediction.h"
#include <algorithm>
#include <cmath>

namespace octaryn::client::app::local_session {
namespace {
constexpr double FixedDt = 1.0 / 60.0;
constexpr size_t HistoryLimit = 256;
character_motion::State state_from(const LocalPlayerPose& pose) {
 return {pose.x, pose.y, pose.z, pose.pitch, pose.yaw,
 pose.velocity_x, pose.velocity_y, pose.velocity_z,
 uint32_t(pose.on_ground), uint32_t(pose.flying), pose.selected_block, uint16_t(pose.jump_held)};
}
}
uint32_t Prediction::query(void* context, int32_t x, int32_t y, int32_t z) {
 auto& self=*static_cast<Prediction*>(context);
 uint32_t packed{};
 if (!self.query_ || !self.query_(self.context_,x,y,z,packed)) {
 self.missing_=true;
 return 1u<<16;
 }
 return packed;
}
bool Prediction::simulate(const PredictionCommand& command) {
 // Check residency even for flight, whose motion kernel does not query blocks.
 missing_=false;
 query(this,int32_t(std::floor(body_.x)),int32_t(std::floor(body_.y-1)),int32_t(std::floor(body_.z)));
 if (missing_) return false;
 auto next=body_;
 character_motion::Input input{command.flags,command.controller,command.moveX,command.moveY,command.moveZ,
 body_.x,body_.y,body_.z,command.cameraPitch,command.cameraYaw,command.relativeMouse};
 character_motion::step(input,float(FixedDt),next,query,this);
 if (command.flags&4u) {
 // Flight bypasses kernel collision; every crossed column still needs residency.
 const int min_x=int(std::floor(std::min(body_.x,next.x)/32));
 const int max_x=int(std::floor(std::max(body_.x,next.x)/32));
 const int min_z=int(std::floor(std::min(body_.z,next.z)/32));
 const int max_z=int(std::floor(std::max(body_.z,next.z)/32));
 for (int z=min_z;z<=max_z && !missing_;++z)
 for (int x=min_x;x<=max_x && !missing_;++x)
 query(this,x*32,int32_t(std::floor(next.y)),z*32);
 query(this,int32_t(std::floor(next.x)),int32_t(std::floor(next.y)),int32_t(std::floor(next.z)));
 }
 if (missing_) return false;
 previous_body_=body_;
 body_=next;
 collision_ready_=true;
 return true;
}
bool Prediction::retry_collision() {
 const auto saved_body=body_, saved_previous=previous_body_;
 const bool saved_ready=collision_ready_;
 body_=state_from(authority_); previous_body_=body_;
 for (const auto& command:pending_) {
 ++replays_;
 if (!simulate(command)) {
 body_=saved_body; previous_body_=saved_previous; collision_ready_=saved_ready;
 return false;
 }
 }
 return true;
}
void Prediction::reconcile(const LocalPlayerPose& pose, uint64_t ack) {
 if (initialized_ && (pose.source_tick<authority_.source_tick || ack<ack_)) return;
 const auto old_render=render_body();
 const auto old_previous=previous_body_;
 const float old_x=old_render.x+correction_x_, old_y=old_render.y+correction_y_, old_z=old_render.z+correction_z_;
 const auto previous_body=body_;
 const float dx=pose.x-authority_.x, dy=pose.y-authority_.y, dz=pose.z-authority_.z;
 const bool teleport=initialized_ && dx*dx+dy*dy+dz*dz>1024.0f;
 if (teleport) {
 collision_ready_=false;
 pending_.clear(); jump_edges_.clear(); accumulator_=0; next_=ack;
 command_jump_=observed_jump_;
 }
 authority_=pose;
 body_=state_from(pose);
 previous_body_=body_;
 ack_=ack;
 next_=std::max(next_,ack);
 while (!pending_.empty() && pending_.front().frameIndex<=ack) pending_.pop_front();
 blocked_=false; collision_blocked_=false;
 for (const auto& command:pending_) {
 ++replays_;
 if (!simulate(command)) { blocked_=true; collision_blocked_=true; break; }
 }
 if (blocked_) previous_body_=body_;
 else if (initialized_ && !teleport && pending_.empty()) {
 // Rebase the last fixed interval when all its commands were acknowledged.
 previous_body_.x-=previous_body.x-old_previous.x;
 previous_body_.y-=previous_body.y-old_previous.y;
 previous_body_.z-=previous_body.z-old_previous.z;
 }
 const auto new_render=render_body();
 correction_x_=old_x-new_render.x; correction_y_=old_y-new_render.y; correction_z_=old_z-new_render.z;
 correction_distance_=initialized_ ? std::sqrt(
     (previous_body.x-body_.x)*(previous_body.x-body_.x)+
     (previous_body.y-body_.y)*(previous_body.y-body_.y)+
     (previous_body.z-body_.z)*(previous_body.z-body_.z)) : 0;
 max_correction_distance_=std::max(max_correction_distance_,correction_distance_);
 const float distance=std::sqrt(correction_x_*correction_x_+correction_y_*correction_y_+correction_z_*correction_z_);
 if (!initialized_ || teleport || distance>2) correction_x_=correction_y_=correction_z_=0;
 else if (distance>0.001f) ++corrections_;
 initialized_=true;
}
void Prediction::advance(const LocalPlayerInput& input,double elapsed,double pose_age) {
 if (input.has_jump_events && input.jump_events.reset) {
 jump_edges_.clear(); command_jump_=false; observed_jump_=false;
 }
 const auto observe_jump=[&](bool pressed) {
 if (pressed==observed_jump_) return;
 if (jump_edges_.size()<32) { jump_edges_.push_back(pressed); observed_jump_=pressed; }
 else { jump_edges_.clear(); command_jump_=false; observed_jump_=pressed; ++overflows_; }
 };
 if (input.has_jump_events) {
 for (size_t i=0;i<std::min(size_t(input.jump_events.count),input.jump_events.pressed.size());++i)
 observe_jump(input.jump_events.pressed[i] && !input.flying);
 } else {
 observe_jump(input.up && !input.flying);
 }
 if (!initialized_) return;
 const float decay=std::exp(-float(std::min(elapsed,0.25))*20.0f);
 correction_x_*=decay; correction_y_*=decay; correction_z_*=decay;
 // Look is presentation-local and never waits for a network acknowledgement.
 if (std::isfinite(input.yaw)) body_.yaw=input.yaw;
 if (std::isfinite(input.pitch)) body_.pitch=std::clamp(input.pitch,-1.55f,1.55f);
 if (pose_age>2.0) {
 blocked_=true; previous_body_=body_; accumulator_=0;
 return;
 }
 if (pending_.size()>=HistoryLimit) {
 accumulator_+=std::min(elapsed,8.0/60.0);
 if (accumulator_>=FixedDt) {
 accumulator_=std::fmod(accumulator_,FixedDt);
 if (collision_blocked_) collision_blocked_=!retry_collision();
 }
 blocked_=true; previous_body_=body_; ++overflows_;
 return;
 }
 accumulator_+=std::min(elapsed,8.0/60.0);
 for (unsigned steps=0; accumulator_>=FixedDt && steps<8 && pending_.size()<HistoryLimit; ++steps) {
 accumulator_-=FixedDt;
 if (!jump_edges_.empty()) { command_jump_=jump_edges_.front(); jump_edges_.pop_front(); }
 PredictionCommand command;
 command.frameIndex=++next_;
 command.flags=(command_jump_?1u:0u)|(input.sprint?2u:0u)|(input.flying?4u:0u);
 command.moveX=float(input.right)-float(input.left);
 command.moveY=input.flying?float(input.up)-float(input.down):0;
 command.moveZ=float(input.forward)-float(input.backward);
 command.cameraPitch=body_.pitch; command.cameraYaw=body_.yaw;
 pending_.push_back(command);
 if (collision_blocked_) collision_blocked_=!retry_collision();
 else collision_blocked_=!simulate(command);
 blocked_=collision_blocked_;
 if (blocked_) previous_body_=body_;
 }
}
character_motion::State Prediction::render_body() const {
 auto result=body_;
 const float alpha=float(std::clamp(accumulator_/FixedDt,0.0,1.0));
 result.x=std::lerp(previous_body_.x,body_.x,alpha);
 result.y=std::lerp(previous_body_.y,body_.y,alpha);
 result.z=std::lerp(previous_body_.z,body_.z,alpha);
 return result;
}
bool Prediction::sample_physics(LocalPlayerPose& pose) const {
 if (!initialized_||!collision_ready_) return false;
 pose=authority_;
 pose.x=body_.x; pose.y=body_.y; pose.z=body_.z;
 pose.yaw=body_.yaw; pose.pitch=body_.pitch;
 pose.velocity_x=body_.velocity_x; pose.velocity_y=body_.velocity_y; pose.velocity_z=body_.velocity_z;
 pose.on_ground=body_.is_on_ground!=0; pose.flying=body_.control_mode==1;
 pose.jump_held=body_.jump_held!=0;
 return true;
}
bool Prediction::sample(LocalPlayerPose& pose) const {
 if (!sample_physics(pose)) return false;
 const auto rendered=render_body();
 pose.x=rendered.x+correction_x_; pose.y=rendered.y+correction_y_; pose.z=rendered.z+correction_z_;
 // Physical contact is immediate; visual landing waits for vertical rollback.
 // Clamp below-floor offsets and the final sub-centimetre positive remainder.
 if (pose.on_ground) {
 if (pose.y<=body_.y+0.005f) pose.y=body_.y;
 else pose.on_ground=false;
 }
 return true;
}
PredictionPacket Prediction::packet() const {
 PredictionPacket result;
 for (const auto& command:pending_) {
 if (result.commands.size()==64) break;
 result.commands.push_back(command);
 }
 return result;
}
LocalMovementStats Prediction::stats() const {
 LocalMovementStats result;
 result.holding=blocked_; result.pending=pending_.size(); result.ack=ack_;
 result.replays=replays_; result.corrections=corrections_; result.overflows=overflows_;
 result.correction_distance=correction_distance_; result.max_correction_distance=max_correction_distance_;
 return result;
}
}
