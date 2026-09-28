#include "TileRayWorkBudget.h"
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace octaryn::client::rendering;
namespace {
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
}
int main() try {
  require(tile_ray_capacity_valid(1) && tile_ray_capacity_valid(4),"diagnostic capacity rejected");
  for(unsigned value:{0u,2u,3u,5u,UINT32_MAX})require(!tile_ray_capacity_valid(value),"unsupported capacity accepted");
  TileRayWorkBudget serial{.2,1};
  require(serial.poll() && !serial.poll() && !serial.allows(MapRayStep::Build,1,0),"serial control ignored capacity");
  TileRayWorkBudget poll{.2};
  for(unsigned i=0;i<4;++i)require(poll.poll(),"bounded poll refused active slot");
  require(!poll.poll(),"unbounded fifth lifecycle poll");
  for(auto step:{MapRayStep::BuildFence,MapRayStep::CompactFence,MapRayStep::CompactAllocation,MapRayStep::Ready})
    require(!poll.allows(step,1,0),"wait/publication misclassified as GPU work");
  require(!poll.allows(MapRayStep::Build,4,0),"fifth lifecycle admitted");
  require(poll.allows(MapRayStep::CompactSubmit,4,0),"full lifecycle slots prevented existing completion");
  require(!poll.allows(MapRayStep::Build,0,.2) &&
      !poll.allows(MapRayStep::Build,0,std::numeric_limits<double>::quiet_NaN()),"CPU admission ignored limit");
  poll.record(MapRayStep::CompactAllocate);
  require(poll.operations==1 && poll.submissions==0 && !poll.allows(MapRayStep::Build,0,0),
      "allocation either invisible to operation cap or falsely counted as submission");
  TileRayWorkBudget submit{.2};submit.record(MapRayStep::Build);
  require(submit.submissions==1 && !submit.allows(MapRayStep::CompactSubmit,1,0),"two submissions in one pump");
  // A permanently pending first tile must not consume another tile's work quota.
  const std::array<MapRayStep,4> steps{MapRayStep::BuildFence,MapRayStep::CompactAllocation,
      MapRayStep::CompactSubmit,MapRayStep::CompactAllocate};
  TileRayWorkBudget progress{.2};unsigned selected=99;
  for(unsigned i=0;i<steps.size();++i) {
    require(progress.poll(),"active lifecycle missing poll");
    if(progress.allows(steps[i],4,.01)) {selected=i;progress.record(steps[i]);}
  }
  require(selected==2 && progress.polls==4 && progress.operations==1 && progress.submissions==1,
      "waiting first tile monopolized work or completion polling stopped after submission");
  // Cancelled GPU work remains a slot until its real fence retires.
  TileRayWorkBudget retirement{.2};unsigned active=3,retired_pending=1;
  require(!retirement.allows(MapRayStep::Build,active+retired_pending,0),"cancel released in-flight capacity early");
  retired_pending=0;
  require(retirement.allows(MapRayStep::Build,active+retired_pending,0),"retired fence did not release capacity");
  std::puts("tile_ray_schedule_tests passed=1 max_inflight=4 max_operations=1 max_submissions=1");return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}
