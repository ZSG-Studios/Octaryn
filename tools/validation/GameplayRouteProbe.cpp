#include "GameplayRoute.h"
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace octaryn::client::app {
static LocalPlayerPose authority;
struct LocalSession::State {};
LocalSession::LocalSession():state_(std::make_unique<State>()){}
LocalSession::~LocalSession()=default;
bool LocalSession::authority_pose(LocalPlayerPose& pose,std::uint64_t& ack) const {pose=authority;ack=authority.source_tick;return true;}
std::uint64_t LocalSession::last_sent_input_frame() const {return authority.source_tick+1;}
LocalMovementStats LocalSession::movement_stats() const {return {};}
}
static void environment(const char* key,const char* value) {
#if defined(_WIN32)
  _putenv_s(key,value);
#else
  setenv(key,value,1);
#endif
}
int main(int argc,char** argv) {
  using namespace octaryn::client::app;
  if(argc!=2)return 1;
  const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
  const auto route=directory/"route.json",csv=directory/"route.csv";
  environment("OCTARYN_CLIENT_GAMEPLAY_ROUTE",route.string().c_str());
  environment("OCTARYN_CLIENT_GAMEPLAY_ROUTE_PATH",csv.string().c_str());
  std::ofstream(route)<<R"({"version":1,"phases":[{"name":"walk","seconds":1.5,"forward":1,"min_distance":1},{"name":"turn","seconds":1,"turn":1},{"name":"waypoint","seconds":1.5,"forward":1,"target":[0,-2.5],"tolerance":0.2}]})";
  bool rejected=false;
  try {GameplayRoute invalid(false,10,0);}catch(const std::runtime_error&){rejected=true;}
  if(!rejected)throw std::runtime_error("Visible route injection was allowed");
  {
    GameplayRoute fixture(true,10,0);LocalSession session;LocalPlayerInput input;
    fixture.apply(input,0,true,session);fixture.record(0,session);
    if(!input.forward || input.flying)throw std::runtime_error("Walk command changed");
    authority.z=-1;authority.source_tick=60;
    fixture.apply(input,1,true,session);fixture.record(1,session);
    fixture.apply(input,.6,true,session);fixture.record(2,session);
    if(input.forward || std::abs(input.yaw-.1f)>.001)throw std::runtime_error("Timed turn changed");
    fixture.apply(input,1,true,session);fixture.record(3,session);
    if(!input.forward || std::abs(input.yaw)>.001)throw std::runtime_error("Waypoint command changed");
    authority.z=-2.5f;authority.source_tick=180;
    fixture.record(4,session);
    fixture.apply(input,1.5,true,session);
    if(!fixture.finish())throw std::runtime_error("Actual authority distance did not satisfy route");
  }
  std::ofstream(route)<<R"({"version":1,"phases":[{"name":"blocked","seconds":1.5,"forward":1,"min_distance":1}]})";
  {
    GameplayRoute fixture(true,10,0);LocalSession session;LocalPlayerInput input;
    fixture.apply(input,0,true,session);fixture.record(0,session);
    fixture.apply(input,1,true,session);fixture.record(1,session);
    fixture.apply(input,1,true,session);
    if(fixture.finish())throw std::runtime_error("Stationary authority falsely passed traversal");
  }
  environment("OCTARYN_CLIENT_GAMEPLAY_ROUTE","");environment("OCTARYN_CLIENT_SCRIPTED_MOVE","1");
  {
    GameplayRoute fixture(true,10,0);LocalSession session;LocalPlayerInput input;input.flying=true;input.yaw=.6f;
    fixture.apply(input,0,true,session);
    if(!input.forward || !input.flying || input.yaw!=.6f)throw std::runtime_error("Legacy movement probe changed controls");
  }
  std::puts("gameplay_route=passed authority_only=1 blocked_rejected=1 timed_turn=1 waypoint=1 hidden_required=1 legacy_preserved=1");
}
