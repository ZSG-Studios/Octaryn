#include "../../../octaryn-client/Source/App/OpenWorld/ResponsiveFrameWait.h"
#include "../../../octaryn-client/Source/App/OpenWorld/PostRenderTrace.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace octaryn::client::app;
namespace {
unsigned checks{};
void check(bool condition,const char* message) {++checks;if(!condition)throw std::runtime_error(message);}
void deadline_contracts() {
  FrameWaitReport report;std::uint64_t clock=100;
  check(wait_frame_deadline(0,[&]{return clock;},[](auto,auto){throw std::runtime_error("Zero duration waited");return FrameWaitResult::Failed;},report),"Zero duration failed");
  check(report.waits==0 && report.requested_ns==0,"Zero duration reported a wait");
  clock=100;unsigned calls{};
  check(wait_frame_deadline(16'666'667,[&]{return clock;},[&](auto remaining,auto timeout){
    check(timeout!=INFINITE && timeout==unsigned((remaining+999'999)/1'000'000),"Unbounded or misrounded wait");
    ++calls;clock+=calls==1?1'000'000:remaining;return FrameWaitResult::Signaled;
  },report),"Early wake failed");
  check(calls==2 && report.actual_ns==16'666'667,"Early wake removed the frame cap");
  clock=0;
  check(wait_frame_deadline(16'666'667,[&]{return clock;},[&](auto,auto){clock+=17'000'000;return FrameWaitResult::Timeout;},report),"Delayed timer signal prevented deadline completion");
  check(report.waits==1 && report.result==FrameWaitResult::Timeout && report.actual_ns==17'000'000,"Timeout/deadline evidence lost");
  clock=0;
  check(wait_frame_deadline(16'666'667,[&]{return clock;},[&](auto,auto){clock+=1'607'000'000;return FrameWaitResult::Timeout;},report),"Overscheduled deadline failed");
  check(report.actual_ns==1'607'000'000,"OS descheduling was hidden from telemetry");
  clock=0;
  check(!wait_frame_deadline(10,[&]{return clock;},[](auto,auto){return FrameWaitResult::Failed;},report),"Failed timer wait was accepted");
  clock=100;
  check(!wait_frame_deadline(10,[&]{return clock;},[&](auto,auto){--clock;return FrameWaitResult::Signaled;},report),"Clock regression accepted");
  clock=0;
  check(!wait_frame_deadline(10,[&]{return clock;},[](auto,auto){return FrameWaitResult::Signaled;},report),"Never advancing clock waited forever");
  check(report.waits==4097,"Early-wake iteration admission changed");
  check(!wait_frame_deadline(1'000'000'001,[&]{return clock;},[](auto,auto){return FrameWaitResult::Failed;},report),"Oversized duration admitted");
}
void actual_timer() {
  ResponsiveFrameWait timer;FrameWaitReport report;
  const auto now=[] {return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());};
  for(const auto duration:std::array<std::uint64_t,4>{1,100,1'000'000,16'666'667}) {
    check(timer.sleep(duration,now,report),"Actual CPU timer failed");
    check(report.actual_ns>=duration && report.last_timeout_ms>=1 && report.last_timeout_ms<=17,"Actual timer violated deadline/finite timeout");
  }
}
void trace_contracts() {
  _putenv_s("OCTARYN_CLIENT_FRAME_CPU_TRACE","1");
  _putenv_s("OCTARYN_CLIENT_FRAME_CPU_TRACE_PATH","frame-cap-cpu.csv");
  {PostRenderTrace trace;trace.begin(365,282);trace.stage("stats_begin");trace.stage("stats_end");trace.stage("cap_sleep_begin",16'666'667);}
  std::ifstream input("frame-cap-cpu.csv.post-render.csv");std::string row,all;
  while(std::getline(input,row))all+=row+'\n';
  check(all.find("365,282,cap_sleep_begin")!=std::string::npos && all.find("16666667")!=std::string::npos,"Pending call trace did not publish requested wait");
  _putenv_s("OCTARYN_CLIENT_FRAME_CPU_TRACE","0");
}
}
int main() {
  try {deadline_contracts();actual_timer();trace_contracts();std::cout<<"frame_cap_wait_checks=passed assertions="<<checks<<" gpu=0 cpu_timer=1 scheduling_guaranteed=0\n";return 0;}
  catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
