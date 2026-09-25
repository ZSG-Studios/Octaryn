#include "PoseHistory.h"
#include <cmath>
#include <initializer_list>
#include <limits>
#include <stdexcept>

namespace {
using namespace octaryn::client::app;
using local_session::PoseHistory;
void require(bool value,const char* reason) {if(!value) throw std::runtime_error(reason);}
void discrete(const LocalPlayerPose& actual,const LocalPlayerPose& expected) {
  require(actual.source_tick==expected.source_tick && actual.on_ground==expected.on_ground &&
      actual.flying==expected.flying,"presentation exposed a different tick's discrete movement state");
}
void transition(LocalPlayerPose a,LocalPlayerPose b) {
  a.source_seconds=10;a.source_tick=100;
  b.source_seconds=10.125;b.source_tick=101;b.x=1;b.y=2;b.z=3;
  PoseHistory history;LocalPlayerPose actual,held;
  require(history.push(a) && history.push(b),"metadata transition setup");
  require(history.sample(actual),"initial sample");discrete(actual,a);
  history.advance(.0625);
  require(history.sample(actual) && actual.source_seconds==10.0625,"midpoint source clock");
  discrete(actual,a);
  require(actual.velocity_x==(a.velocity_x+b.velocity_x)*.5f &&
      actual.velocity_y==(a.velocity_y+b.velocity_y)*.5f &&
      actual.velocity_z==(a.velocity_z+b.velocity_z)*.5f,"velocity must interpolate at presentation cursor");
  held=actual;history.advance(0);require(history.sample(actual),"held interior sample");
  discrete(actual,held);
  require(actual.source_seconds==held.source_seconds && actual.x==held.x &&
      actual.velocity_y==held.velocity_y,"held timestamp must preserve pose and velocity");
  history.advance(.0625);require(history.sample(actual),"exact endpoint sample");discrete(actual,b);
  require(actual.source_seconds==b.source_seconds && actual.x==b.x && actual.y==b.y && actual.z==b.z &&
      actual.velocity_x==b.velocity_x && actual.velocity_y==b.velocity_y && actual.velocity_z==b.velocity_z,
      "endpoint must publish exact authoritative pose and velocity");
  history.advance(.5);require(history.sample(actual),"underrun hold");discrete(actual,b);
  require(actual.source_seconds==b.source_seconds && actual.x==b.x,"underrun must hold endpoint");

  PoseHistory edge;require(edge.push(a) && edge.push(b),"boundary setup");
  const double before=std::nextafter(b.source_seconds,-std::numeric_limits<double>::infinity());
  edge.advance(before-a.source_seconds);require(edge.sample(actual),"immediately before boundary");
  require(actual.source_seconds==before,"double source cursor preserved");discrete(actual,a);
  edge.advance(b.source_seconds-before);require(edge.sample(actual),"boundary crossing");discrete(actual,b);
}

void cadence(int server_hz,int render_hz) {
  PoseHistory history;
  LocalPlayerPose actual;
  int server_packet=-1,delivered=-1;
  double next_poll=0,anchor_wall=-1,anchor_source=0;
  for(int frame=0;frame<render_hz*10;++frame) {
    const double now=static_cast<double>(frame)/render_hz;
    // File-mailbox cadence fixture: sample the latest server state every16.667ms.
    // Publication latency varies0..6ms; the actual PoseHistory consumes latest.
    while(next_poll<=now+1e-10) {
      while(static_cast<double>(server_packet+1)/server_hz+
          static_cast<double>((server_packet+1)%7)*.001<=next_poll+1e-10) ++server_packet;
      delivered=server_packet;next_poll+=.016667;
    }
    if(delivered>=0) {
      LocalPlayerPose packet;
      packet.source_tick=static_cast<uint64_t>(delivered+1);
      packet.source_seconds=1+static_cast<double>(delivered)/server_hz;
      packet.x=static_cast<float>((packet.source_seconds-1)*5);
      packet.velocity_x=5;packet.on_ground=true;
      history.push(packet);
    }
    history.advance(1.0/render_hz);
    if(!history.sample(actual) || history.stats().holding) continue;
    if(anchor_wall<0) {anchor_wall=now;anchor_source=actual.source_seconds;}
    require(std::abs(actual.source_seconds-anchor_source-(now-anchor_wall))<1e-9,
        "jitter or latest-only polling changed1x movement clock");
    require(std::abs(actual.x-static_cast<float>((actual.source_seconds-1)*5))<.00002f,
        "constant-speed movement differs from source-time trajectory");
  }
  require(anchor_wall>=0 && history.stats().underruns==0,
      "normal30/60Hz publication plus60Hz polling must not cause refill holds");
}
}
void check_pose_history_metadata() {
  for(const int render_hz:{60,240,1000}) for(const int server_hz:{30,60}) cadence(server_hz,render_hz);
  LocalPlayerPose ground,jump,fall,flight;
  ground.on_ground=true;ground.velocity_x=5;
  jump.velocity_x=5;jump.velocity_y=8;
  fall.velocity_x=3;fall.velocity_y=-6;fall.velocity_z=2;
  flight.flying=true;flight.velocity_x=10;flight.velocity_y=-2;
  transition(ground,jump);
  transition(fall,ground);
  transition(ground,flight);
  transition(flight,fall);
  auto reversed=ground;reversed.velocity_x=-5;transition(ground,reversed);

  PoseHistory history;LocalPlayerPose a,b,actual;
  a.source_tick=1;a.source_seconds=1;
  b.source_tick=2;b.source_seconds=1.125;b.x=1;
  require(history.push(a) && history.push(b),"refill setup");history.advance(.126);
  require(history.stats().holding && history.stats().underruns==1,"refill setup must create a real overrun");
  a=b;a.source_tick=3;a.source_seconds=1.15625;a.x=2;
  require(history.push(a),"partial refill");history.advance(.01);require(history.sample(actual),"partial refill sample");
  discrete(actual,b);require(actual.source_seconds==b.source_seconds,"partial refill must not advance");
  a.source_tick=4;a.source_seconds=1.1875;a.x=3;require(history.push(a),"full refill");
  history.advance(.015625);require(history.sample(actual),"refill playback");
  require(actual.source_seconds==1.140625,"refill playback must remain 1x");discrete(actual,b);
  require(!history.push(b),"stale source rejection unchanged");
  a.source_tick=5;a.source_seconds=1.25;a.x=100;
  require(history.push(a) && history.sample(actual),"teleport reset");discrete(actual,a);
  require(actual.x==100 && actual.source_seconds==1.25,"teleport must snap and reset cursor");

  PoseHistory boundary;
  a={};a.source_tick=1;a.source_seconds=1;
  b=a;b.source_tick=2;b.source_seconds=1.0625;b.x=.3125f;
  require(boundary.push(a) && boundary.push(b),"endpoint cadence setup");
  boundary.advance(.0625);
  require(boundary.stats().underruns==0 && !boundary.stats().holding,
      "exact valid endpoint must not trigger an artificial refill hold");
  b.source_tick=3;b.source_seconds=1.09375;b.x=.46875f;
  require(boundary.push(b),"next timely snapshot");boundary.advance(.015625);
  require(boundary.sample(actual) && actual.source_seconds==1.078125,
      "timely next snapshot must continue1x without waiting another50ms");
  boundary.advance(1);
  require(boundary.stats().underruns==1 && boundary.stats().holding,
      "real outage must still hold and count one underrun");
  boundary.advance(1);
  require(boundary.stats().underruns==1,"held frames must not count new outages");
}
