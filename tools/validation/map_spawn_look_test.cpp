#include "Prediction.h"
#include <cmath>
#include <cstdio>
#include <limits>
using namespace octaryn::client::app;
using octaryn::client::app::local_session::Prediction;
// The physics test double holds position; this regression tests real prediction
// look initialization and outgoing commands, not collision or movement.
namespace octaryn::character_motion {
void step(const Input&,float,State&,SolidQuery,void*) {}
}
bool resident(void*,int32_t,int32_t,int32_t,uint32_t& packed) {packed=0;return true;}
int main() {
    int checks=0,failures=0;
    auto check=[&](bool value){++checks;if(!value)++failures;};
    LocalPlayerPose spawn{};spawn.yaw=.6f;spawn.pitch=-.25f;spawn.source_tick=1;
    Prediction prediction;
    prediction.set_collision(resident,nullptr);
    LocalPlayerInput waiting{};
    waiting.yaw=waiting.pitch=std::numeric_limits<float>::quiet_NaN();
    prediction.advance(waiting,1.0/30,0);
    LocalPlayerPose pose{};
    check(!prediction.sample(pose));
    prediction.reconcile(spawn,0);
    prediction.advance(waiting,1.0/30,0);
    check(prediction.sample(pose));
    check(pose.yaw==spawn.yaw && pose.pitch==spawn.pitch);
    auto packet=prediction.packet();
    check(!packet.commands.empty());
    for(const auto& command:packet.commands) {
        check(std::isfinite(command.cameraYaw) && std::isfinite(command.cameraPitch));
        check(command.cameraYaw==spawn.yaw && command.cameraPitch==spawn.pitch);
    }
    LocalPlayerInput active{};active.yaw=.8f;active.pitch=-.3f;
    prediction.advance(active,1.0/60,0);prediction.sample(pose);
    check(pose.yaw==active.yaw && pose.pitch==active.pitch);
    Prediction oldOrder;oldOrder.set_collision(resident,nullptr);oldOrder.reconcile(spawn,0);
    LocalPlayerInput defaults{};defaults.pitch=-.15f;
    oldOrder.advance(defaults,1.0/30,0);oldOrder.sample(pose);
    check(pose.yaw==0 && pose.pitch==-.15f);
    std::printf("map spawn look: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
