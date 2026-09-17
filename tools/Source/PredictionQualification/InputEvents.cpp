#include "JumpInputEvents.h"
#include "Prediction.h"
#include <cstdio>
#include <cstdlib>

using namespace octaryn::client::app;
using local_session::Prediction;
namespace {
constexpr double Tick=1.0/60;
void check(bool value,const char* message) {
 if (!value) { std::fprintf(stderr,"input_events_failed %s\n",message); std::exit(1); }
}
bool ground(void*,int32_t,int32_t y,int32_t,uint32_t& packed) {
 packed=y<0?((1u<<16)|1u):0; return true;
}
void initialize(Prediction& prediction) {
 prediction.set_collision(ground,nullptr);
 LocalPlayerPose spawn;spawn.y=1.62f;spawn.on_ground=true;
 prediction.reconcile(spawn,0);
}
SDL_Event key(bool down,bool repeat=false) {
 SDL_Event event{};
 event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
 event.key.scancode=SDL_SCANCODE_SPACE;event.key.key=SDLK_SPACE;
 event.key.repeat=repeat;
 return event;
}
LocalPlayerInput drain(JumpInputEvents& events) {
 LocalPlayerInput input;
 input.has_jump_events=true;input.jump_events=events.take();
 return input;
}
}
void qualify_input_events() {
 JumpInputEvents events;
 Prediction prediction;initialize(prediction);
 events.event(key(true),true);events.event(key(false),true);
 auto input=drain(events);
 check(!input.up && input.jump_events.count==2,"same-frame tap survives final keyboard-up state");
 prediction.advance(input,1.0/30,0);
 auto packet=prediction.packet();
 check(packet.commands.size()==2 && (packet.commands[0].flags&1) && !(packet.commands[1].flags&1),
 "30 Hz frame emits exactly one press followed by release");
 LocalPlayerPose physics;prediction.sample_physics(physics);
 check(physics.velocity_y>0 && !physics.jump_held,"short tap takes off and releases without acknowledgement");

 JumpInputEvents ui;
 Prediction suppressed;initialize(suppressed);
 ui.event(key(true),false);ui.event(key(false),false);
 input=drain(ui);suppressed.advance(input,1.0/30,0);
 packet=suppressed.packet();suppressed.sample_physics(physics);
 check(input.jump_events.count==0 && !(packet.commands[0].flags&1) && physics.on_ground,
 "UI-consumed down/up cannot jump");

 JumpInputEvents held;
 Prediction hold;initialize(hold);
 held.event(key(true),true);input=drain(held);hold.advance(input,Tick,0);
 for(int i=0;i<100;++i) {
 held.event(key(true,true),true);held.event(key(true),true);
 input=drain(held);check(input.jump_events.count==0,"repeat and duplicate keydown ignored");
 hold.advance(input,Tick,0);
 }
 hold.sample_physics(physics);
 check(physics.on_ground && physics.jump_held,"held key lands without automatic repeat jumps");

 JumpInputEvents focus;
 Prediction cancelled;initialize(cancelled);
 focus.event(key(true),true);cancelled.advance(drain(focus),Tick/2,0);
 SDL_Event lost{};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;
 focus.event(lost,false);cancelled.advance(drain(focus),Tick/2,0);
 check(!(cancelled.packet().commands[0].flags&1),"focus loss cancels queued unsimulated press");
 focus.event(key(true,true),true);
 check(drain(focus).jump_events.count==0,"focus regain repeat cannot recreate jump");

 JumpInputEvents flight;
 Prediction flying;initialize(flying);
 flight.event(key(true),false);input=drain(flight);input.flying=true;input.up=true;
 flying.advance(input,Tick,0);packet=flying.packet();
 check(packet.commands[0].moveY==1 && !(packet.commands[0].flags&1),"flight ascent remains independent of walk jump edges");

 JumpInputEvents menu;
 menu.event(key(true),false);
 menu.event(key(true),true);
 check(drain(menu).jump_events.count==0,"menu-held key cannot jump when UI closes");
 menu.event(key(false),true);menu.event(key(true),true);
 check(drain(menu).jump_events.count==1,"fresh press after UI release is accepted");
 JumpInputEvents burst;
 for(int i=0;i<40;++i)burst.event(key((i&1)==0),true);
 const auto bounded=burst.take();
 check(burst.overflows()==1 && bounded.count<=32 && bounded.reset,"overflow remains bounded and cancels stale held intent");
 std::puts("prediction_input_events same_frame_30Hz=press/release ui=blocked held_repeat=none focus=cancelled flight=preserved");
}
