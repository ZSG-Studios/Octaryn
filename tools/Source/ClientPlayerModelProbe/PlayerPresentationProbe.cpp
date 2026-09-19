#include "PlayerView.h"
#include "CameraBoom.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <cstdio>

namespace {
void check_shoulder_camera() {
  using namespace octaryn::client;
  using app::CameraShoulder;
  unsigned checks{};
  const auto require=[&](bool pass,const char* text){++checks;if(!pass)throw std::runtime_error(text);};
  const auto air=[](int,int,int){return false;};
  require(app::WorldControls{}.shoulder==CameraShoulder::Right,"default shoulder must be right");
  require(app::opposite_shoulder(CameraShoulder::Left)==CameraShoulder::Right &&
      app::opposite_shoulder(CameraShoulder::Right)==CameraShoulder::Left,"shoulder swap must have only two states");
  for(float yaw:{0.f,1.5707963f,-1.5707963f,3.1415926f})for(float pitch:{-1.4f,-.4f,0.f,.4f,1.4f}) {
    rendering::WorldCamera eye{-32.5f,10.5f,-.5f,yaw,pitch,.6f};
    const auto left=app::shoulder_camera(eye,CameraShoulder::Left,air);
    const auto right=app::shoulder_camera(eye,CameraShoulder::Right,air);
    const double lateral=(right.x-left.x)*std::cos(yaw)+(right.z-left.z)*std::sin(yaw);
    const double backward=-(right.x-eye.x)*std::sin(yaw)*std::cos(pitch)-
        (right.y-eye.y)*std::sin(pitch)+(right.z-eye.z)*std::cos(yaw)*std::cos(pitch);
    require(std::abs(lateral-1.485)<1e-4,"left/right camera separation must follow horizontal right basis");
    require(std::abs(backward-3.96)<1e-4,"shoulder offset must preserve existing backward distance");
    require(std::abs(left.y-right.y)<1e-5 && right.yaw==eye.yaw && right.pitch==eye.pitch &&
        right.vertical_fov==eye.vertical_fov,"shoulder must preserve view angles, zoom and vertical offset");
  }
  const rendering::WorldCamera eye{.5f,10.5f,.5f,0,0,1};
  const auto clear_right=app::shoulder_camera(eye,CameraShoulder::Right,air);
  const auto clear_left=app::shoulder_camera(eye,CameraShoulder::Left,air);
  const auto right_wall=[](int x,int y,int z){return x==1 && y==10 && z==3;};
  const auto hit_right=app::shoulder_camera(eye,CameraShoulder::Right,right_wall);
  const auto miss_left=app::shoulder_camera(eye,CameraShoulder::Left,right_wall);
  require(hit_right.z<2.82f && hit_right.z>eye.z && hit_right.x<clear_right.x,
      "combined boom must retract before right-side obstruction");
  require(miss_left.x==clear_left.x && miss_left.z==clear_left.z,"right obstacle must not retract left shoulder");
  const auto left_wall=[](int x,int y,int z){return x==-1 && y==10 && z==3;};
  const auto hit_left=app::shoulder_camera(eye,CameraShoulder::Left,left_wall);
  require(hit_left.z<2.82f && hit_left.z>eye.z && hit_left.x>clear_left.x,
      "combined boom must retract before left-side obstruction");
  const auto obstructed=app::shoulder_camera(eye,CameraShoulder::Right,[](int,int,int){return true;});
  require(obstructed.x==eye.x && obstructed.y==eye.y && obstructed.z==eye.z,
      "camera inside solid or unavailable world must remain at eye");
  const auto rear=app::shoulder_camera(eye,CameraShoulder::Left,[](int,int,int z){return z==2;});
  require(rear.z<1.82f && rear.z>eye.z,"rear wall must still clip offset camera");
  std::printf("player_shoulder_camera=passed checks=%u sides=2 default=right sweep=combined\n",checks);
}
}

void check_player_presentation() {
  check_shoulder_camera();
  using namespace octaryn::client;
  using Clip=rendering::PlayerClip;
  app::LocalPlayerPose pose;pose.on_ground=true;pose.source_seconds=10;
  app::WorldControls controls;rendering::WorldCamera camera;
  auto expect=[&](Clip expected,double attack_until=0,uint64_t sequence=0) {
    const auto actual=app::player_presentation(pose,controls,camera,pose.source_seconds,attack_until,sequence);
    if(actual.clip!=expected || actual.source_seconds!=pose.source_seconds ||
       actual.action_sequence!=(expected==Clip::Attack?sequence:0))
      throw std::runtime_error("player presentation disagrees with authoritative movement/attack state");
  };
  expect(Clip::Idle);
  pose.velocity_x=5;expect(Clip::Walk);
  const float infinity=std::numeric_limits<float>::infinity();
  pose.velocity_x=std::nextafter(5.0f,infinity);expect(Clip::Walk);
  pose.velocity_x=std::nextafter(7.0f,-infinity);expect(Clip::Walk);
  pose.velocity_x=7;expect(Clip::Walk);
  pose.velocity_x=std::nextafter(7.0f,infinity);expect(Clip::Run);
  pose.velocity_x=9;expect(Clip::Run);
  // Match authority's float position-difference / dt reporting. A constant walk
  // crosses either side of 5 numerically at ordinary nearby world coordinates.
  bool above_walk=false,below_walk=false;
  for(const float frequency:{30.0f,60.0f}) {
    const float dt=1.0f/frequency;
    float position=.5f;
    for(unsigned step=0;step<32;++step) {
      const float next=position+5.0f*dt;
      pose.velocity_x=(next-position)/dt;
      above_walk|=pose.velocity_x>5;below_walk|=pose.velocity_x<5;
      expect(Clip::Walk);
      position=next;
    }
  }
  if(!above_walk || !below_walk) throw std::runtime_error("quantized walk fixture must exercise both sides of nominal speed");
  controls.movement.move_down=1;
  pose.velocity_x=0;expect(Clip::Idle);
  pose.velocity_x=5;expect(Clip::Walk);
  pose.velocity_x=9;expect(Clip::Run);
  pose.on_ground=false;pose.velocity_y=8;expect(Clip::Jump);
  pose.velocity_y=-3;expect(Clip::Fall);
  pose.flying=true;pose.velocity_x=0;expect(Clip::Idle);
  pose.velocity_x=10;expect(Clip::Run);
  expect(Clip::Attack,11,42);
  pose.flying=false;expect(Clip::Attack,11,43);
  expect(Clip::Fall,10,43); // Exact expiry returns to authoritative airborne state.
  pose.on_ground=true;expect(Clip::Attack,11,44);
}
