#include "AuthorityTick.h"
#include "octaryn_native_schedule_policy.h"
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>

struct Context {
  std::thread::id owner=std::this_thread::get_id();
  std::vector<unsigned> phases;
  int fail_phase=-1;
};
static int phase(Context& context,unsigned index) {
  if(std::this_thread::get_id()!=context.owner)throw std::runtime_error("Authority left its owner thread");
  context.phases.push_back(index);
  return int(index)==context.fail_phase?-17:0;
}
static int commands(void* pointer) {return phase(*static_cast<Context*>(pointer),0);}
static int player(void* pointer) {return phase(*static_cast<Context*>(pointer),1);}
static int time(void* pointer) {return phase(*static_cast<Context*>(pointer),2);}
static int scoped(void* pointer) {
  auto& context=*static_cast<Context*>(pointer);
  if(!octaryn_native_command_write_scope_is_active())throw std::runtime_error("Command scope missing");
  return phase(context,3);
}
int main() {
  auto* runtime=octaryn_native_schedule_runtime_create(2,2);
  if(!runtime)return 1;
  Context context;
  const octaryn_server_authority_tick_callbacks callbacks{commands,&context,player,&context,time,&context};
  for(unsigned repeat=0;repeat<10000;++repeat) {
    context.phases.clear();
    octaryn_native_schedule_runtime_report report{};
    if(octaryn_server_authority_tick_execute(runtime,&callbacks,&report)!=0 ||
        octaryn_server_authority_tick_validate_report(&report)!=0 ||
        context.phases!=std::vector<unsigned>{0,1,2})throw std::runtime_error("Authority order/report changed");
  }
  for(int fail=0;fail<3;++fail) {
    context.phases.clear();context.fail_phase=fail;
    octaryn_native_schedule_runtime_report report{};
    if(octaryn_server_authority_tick_execute(runtime,&callbacks,&report)!=-17 ||
        report.completed_jobs!=0 || report.failed_job_index!=0 || context.phases.size()!=unsigned(fail+1))
      throw std::runtime_error("Authority failure continued dependent phases");
  }
  context.fail_phase=3;context.phases.clear();
  const octaryn_native_schedule_runtime_job module{"probe.owner.module",nullptr,0,nullptr,0,
    OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_MAIN_THREAD|OCTARYN_NATIVE_SCHEDULE_RUNTIME_JOB_COMMAND_WRITE,scoped,&context};
  if(octaryn_native_schedule_runtime_execute(runtime,&module,1,nullptr)!=-17 ||
      octaryn_native_command_write_scope_is_active())throw std::runtime_error("Owner command scope leaked after failure");
  octaryn_native_schedule_runtime_destroy(runtime);
  std::puts("authority_ordering=passed owner_thread=1 iterations=10000 fail_fast_phases=3 command_scope_restored=1");
}
