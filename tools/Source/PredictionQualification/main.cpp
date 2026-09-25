#include "Prediction.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace octaryn::client::app;
using octaryn::client::app::local_session::Prediction;
void qualify_input_events();
namespace {
constexpr double Dt=1.0/60;
void require(bool value,const char* message) { if (!value) { std::fprintf(stderr,"prediction_failed %s\n",message); std::exit(1); } }
bool ground(void*,int32_t,int32_t y,int32_t,uint32_t& packed) { packed=y<0?((1u<<16)|1u):0; return true; }
bool missing(void*,int32_t,int32_t,int32_t,uint32_t&) { return false; }
LocalPlayerPose spawn() { LocalPlayerPose pose; pose.y=1.62f;pose.on_ground=true; return pose; }
void smooth_render(int hz) {
 Prediction prediction;prediction.set_collision(ground,nullptr);
 auto initial=spawn();initial.y=20;initial.flying=true;initial.on_ground=false;
 prediction.reconcile(initial,0);
 LocalPlayerInput input;input.flying=true;input.forward=true;
 LocalPlayerPose pose,previous;
 unsigned changes{},intermediate{};
 for(int frame=0;frame<hz;++frame) {
 input.yaw=0;
 prediction.advance(input,1.0/hz,0);
 if(!prediction.sample(pose))continue;
 if(frame>5) {
 require(pose.z<previous.z,"high-refresh movement advances every frame");
 require(std::abs((pose.z-previous.z)+10.0f/float(hz))<0.0001f,"constant render speed between ticks");
 ++changes;
 if(std::abs(pose.z*6-std::round(pose.z*6))>0.001f)++intermediate;
 }
 previous=pose;
 }
 require(changes>unsigned(hz-10) && intermediate>unsigned(hz/2),"144/240 Hz have intermediate positions");
 input.yaw=1.25f;prediction.advance(input,0,0);prediction.sample(pose);
 require(pose.yaw==1.25f,"look remains immediate at zero simulation delta");
 Prediction jumper;jumper.set_collision(ground,nullptr);jumper.reconcile(spawn(),0);
 input={};for(int frame=0;frame<10;++frame)jumper.advance(input,1.0/hz,0);
 jumper.sample(previous);input.up=true;
 double onset{};
 for(int frame=1;frame<10;++frame) {
 jumper.advance(input,1.0/hz,0);jumper.sample(pose);
 if(pose.y>previous.y+0.001f) {onset=double(frame)/hz;break;}
 }
 require(onset>0 && onset<=2*Dt,"visible jump within two fixed ticks at high refresh");
 std::printf("prediction_render hz=%d intermediate=%u jump_onset_ms=%.3f\n",hz,intermediate,onset*1000);
}
void acknowledged_interval() {
 Prediction prediction;prediction.set_collision(ground,nullptr);
 auto initial=spawn();initial.y=20;initial.flying=true;initial.on_ground=false;
 prediction.reconcile(initial,0);
 LocalPlayerInput input;input.flying=true;input.forward=true;
 for(int i=0;i<24;++i)prediction.advance(input,1.0/240,0);
 LocalPlayerPose before,after;prediction.sample(before);
 auto ack=initial;
 const auto count=prediction.stats().pending;
 ack.z=-float(count)*10.0f/60;ack.velocity_z=-10;
 ack.source_tick=count;ack.source_seconds=double(count)/60;
 prediction.reconcile(ack,count);prediction.sample(after);
 require(prediction.stats().pending==0 && prediction.stats().correction_distance<0.0001f,"all-acked physical endpoint");
 require(std::abs(after.z-before.z)<0.0001f,"all-acked interpolation continuity without double correction");
 prediction.advance(input,1.0/240,0);prediction.sample(after);
 require(std::abs(after.z-before.z+10.0f/240)<0.0001f,"all-acked render interval continues at constant speed");
 require(after.velocity_z==-10 && !after.on_ground,"render interpolation preserves current physics metadata");
}
void grounded_rollback() {
 Prediction prediction;prediction.set_collision(ground,nullptr);prediction.reconcile(spawn(),0);
 LocalPlayerInput input;input.up=true;
 for(int i=0;i<12;++i)prediction.advance(input,Dt,0);
 LocalPlayerPose before,render,physics;prediction.sample(before);
 require(before.y>2.2f,"rollback starts visibly airborne");
 auto rejected=spawn();rejected.source_tick=12;rejected.source_seconds=12*Dt;
 rejected.jump_held=true;
 prediction.reconcile(rejected,12);
 prediction.sample(render);prediction.sample_physics(physics);
 require(physics.on_ground && physics.y==1.62f && physics.velocity_y==0,"authoritative grounded state restored immediately");
 require(!render.on_ground && std::abs(render.y-before.y)<0.001f,"rollback starts continuously without floating landed flag");
 float previous_y=render.y;bool landed=false;
 for(int i=0;i<144;++i) {
 prediction.advance(input,1.0/240,0);
 prediction.sample(render);prediction.sample_physics(physics);
 require(physics.on_ground && physics.velocity_y==0,"held rejected jump cannot trigger a second takeoff");
 require(render.y<=previous_y+0.0001f && previous_y-render.y<0.12f,"ground correction descends smoothly without upward rebound");
 if(render.on_ground) {
 require(render.y==physics.y,"presented landing is at the physical floor");
 landed=true;
 }
 previous_y=render.y;
 }
 require(landed,"ground correction settles in bounded time");
 input.up=false;prediction.advance(input,Dt,0);
 input.up=true;prediction.advance(input,Dt,0);prediction.sample_physics(physics);
 require(physics.velocity_y>0 && !physics.on_ground,"a new release/press can jump immediately after rollback");
 auto raised=spawn();raised.y=3.62f;raised.source_tick=100;raised.jump_held=true;
 prediction.reconcile(raised,100);prediction.sample(render);prediction.sample_physics(physics);
 require(render.y>=physics.y && render.on_ground,"grounded correction cannot render below its authoritative floor");
 std::puts("prediction_grounded_rollback physical_floor=immediate visual_landing=at_floor double_jump=none");
}
struct Residency { bool available{}; };
bool boundary(void* context,int32_t x,int32_t y,int32_t z,uint32_t& packed) {
 if(x>=32 && !static_cast<Residency*>(context)->available)return false;
 return ground(nullptr,x,y,z,packed);
}
void collision_recovery() {
 Residency terrain;
 Prediction flight;flight.set_collision(boundary,&terrain);
 auto initial=spawn();initial.x=31.5f;initial.y=20;initial.flying=true;
 flight.reconcile(initial,0);
 LocalPlayerInput input;input.flying=true;
 flight.advance(input,Dt,0);
 LocalPlayerPose pose;require(flight.sample(pose),"known baseline");
 input.right=true;input.sprint=true;
 for(int i=0;i<4;++i)flight.advance(input,Dt,0);
 flight.sample(pose);
 require(pose.x==31.5f && flight.stats().holding,"flight cannot cross missing destination column");
 terrain.available=true;
 flight.advance(input,Dt,0);flight.sample(pose);
 require(pose.x>32 && !flight.stats().holding && flight.stats().ack==0,"residency recovery replays without snapshot or ack");
 const auto stats=flight.stats();
 require(stats.pending==6 && stats.replays>0,"recovery preserves command history");
 initial.x=1000;initial.source_tick=100;
 flight.reconcile(initial,0);input={};input.flying=true;flight.advance(input,Dt,0);flight.sample(pose);
 require(pose.x==1000,"teleport resets interpolation endpoints and correction");
 Prediction unknown;unknown.set_collision(missing,nullptr);unknown.reconcile(spawn(),0);
 input={};input.up=true;
 for(int i=0;i<4;++i)unknown.advance(input,Dt,0);
 require(!unknown.sample(pose),"unknown initial collision cannot establish baseline");
 unknown.set_collision(ground,nullptr);
 unknown.advance(input,Dt,0);
 require(unknown.sample(pose) && pose.velocity_y>0 && pose.y>1.62f,"missing-baseline jump survives local-only recovery");
}
}
int main() {
 qualify_input_events();
 Prediction prediction;
 prediction.set_collision(ground,nullptr);prediction.reconcile(spawn(),0);
 LocalPlayerInput input;LocalPlayerPose pose;
 input.up=true;prediction.advance(input,Dt,0);
 require(prediction.sample(pose),"spawn");
 require(pose.velocity_y>0 && !pose.on_ground,"jump physics within one tick before ack");
 const auto first=prediction.packet();
 require(first.commands.size()==1 && (first.commands[0].flags&1),"first jump command");
 input.up=false;
 for(int i=0;i<100;++i)prediction.advance(input,Dt,0);
 prediction.sample(pose);require(pose.on_ground,"land without ack");
 input.up=true;prediction.advance(input,Dt,0);prediction.sample(pose);
 require(pose.velocity_y>0 && !pose.on_ground,"second jump not ack gated");
 require(prediction.packet().commands.size()==64,"bounded oldest resend batch");
 // Authoritative snapshots contain physics state, never interpolated render state.
 octaryn::character_motion::State body{0,1.62f,0,0,0,0,0,0,1,0,0,0};
 octaryn::character_motion::Input command{};command.flags=1;command.controller=1;command.relative_mouse=1;
 octaryn::character_motion::step(command,float(Dt),body,
 [](void*,int32_t,int32_t y,int32_t)->uint32_t {return y<0?((1u<<16)|1u):0;},nullptr);
 auto ack=spawn();ack.x=body.x;ack.y=body.y;ack.z=body.z;
 ack.velocity_y=body.velocity_y;ack.on_ground=body.is_on_ground!=0;ack.jump_held=body.jump_held!=0;
 ack.source_tick=1;ack.source_seconds=Dt;
 const auto before=pose;prediction.reconcile(ack,1);prediction.sample(pose);
 require(std::abs(pose.y-before.y)<0.002f,"render continuity across reconcile");
 require(prediction.stats().correction_distance<0.002f,"physical restore/replay parity independent of interpolation");
 require(prediction.stats().ack==1 && prediction.stats().pending==101,"discard only consumed command");
 auto stale=spawn();prediction.reconcile(stale,0);
 require(prediction.stats().ack==1,"regressing ack ignored");
 for(int i=0;i<400;++i)prediction.advance(input,Dt,0);
 require(prediction.stats().pending==256 && prediction.stats().overflows>0,"bounded outage history");
 Prediction edges;edges.set_collision(ground,nullptr);edges.reconcile(spawn(),0);
 edges.advance(input,Dt/4,0);input.up=false;edges.advance(input,Dt/4,0);
 edges.advance(input,Dt/2,0);edges.advance(input,Dt,0);
 const auto packet=edges.packet();
 require(packet.commands.size()==2 && (packet.commands[0].flags&1) && !(packet.commands[1].flags&1),"subtick edges retained");
 smooth_render(144);smooth_render(240);acknowledged_interval();collision_recovery();grounded_rollback();
 std::puts("prediction_qualification passed fixed_physics=60Hz render=144/240Hz replay_parity=passed collision_recovery=passed");
}
