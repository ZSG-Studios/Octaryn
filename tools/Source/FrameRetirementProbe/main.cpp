#define OCTARYN_PROFILE_TESTS 1
#include "FrameCpuTrace.h"
#include "FrameFenceWait.h"
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace octaryn::client::rendering;
namespace octaryn::client::diagnostics {
struct AsyncProfileTestAccess {
  static void hooks(AsyncProfileStream& stream,std::function<void()> write,std::function<void()> close={}) {
    stream.buffer_.before_write_=std::move(write);stream.buffer_.before_close_=std::move(close);
  }
  static void fail_file(AsyncProfileStream& stream) {stream.buffer_.file_.setstate(std::ios::badbit);}
};
}
using octaryn::client::diagnostics::AsyncProfileStream;
using octaryn::client::diagnostics::AsyncProfileTestAccess;
namespace {
void require(bool ok,const char* reason) {if(!ok)throw std::runtime_error(reason);}
struct Gate {
  std::mutex mutex;std::condition_variable cv;bool entered{},released{};
  void block() {std::unique_lock lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[&]{return released;});}
  void await() {std::unique_lock lock(mutex);require(cv.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}),"worker not entered");}
  void release() {std::lock_guard lock(mutex);released=true;cv.notify_all();}
};
void writer_cases(const std::filesystem::path& directory) {
  AsyncProfileStream ordered;const auto path=directory/"ordered.csv";ordered.open(path);
  const auto producer=std::this_thread::get_id();bool worker_thread{};
  AsyncProfileTestAccess::hooks(ordered,[&] {worker_thread=std::this_thread::get_id()!=producer;});
  std::ostringstream expected;
  for(unsigned i=0;i<10000;++i) {ordered<<i<<",0.125\n";expected<<i<<",0.125\n";}
  require(ordered.close() && worker_thread,"write/flush not completed on worker");
  std::ifstream file(path,std::ios::binary);const std::string bytes((std::istreambuf_iterator<char>(file)),{});
  require(bytes==expected.str(),"ordered complete CSV rows changed");
  AsyncProfileStream overflow;Gate gate;
  AsyncProfileTestAccess::hooks(overflow,[&] {gate.block();});overflow.open(directory/"overflow.csv");
  overflow<<"in-flight\n";overflow.flush();gate.await();
  for(unsigned i=0;i<65;++i) {overflow<<i<<'\n';overflow.flush();}
  const bool rejected=!overflow;gate.release();
  require(rejected && !overflow.close(),"queue saturation did not invalidate capture");
  AsyncProfileStream failed_write;
  AsyncProfileTestAccess::hooks(failed_write,[&] {AsyncProfileTestAccess::fail_file(failed_write);});
  failed_write.open(directory/"write-failure.csv");failed_write<<"row\n";failed_write.flush();
  require(!failed_write.close() && !failed_write,"worker write failure not propagated");
  AsyncProfileStream failed_close;
  AsyncProfileTestAccess::hooks(failed_close,{},[&] {AsyncProfileTestAccess::fail_file(failed_close);});
  failed_close.open(directory/"close-failure.csv");failed_close<<"row\n";
  require(!failed_close.close() && !failed_close,"close failure not propagated");
}
void fence_cases() {
  unsigned current_calls{},wait_calls{};std::uint64_t tick{};
  const auto run=[&](std::uint64_t target,std::uint64_t before,std::uint64_t after,SlangResult waiting=SLANG_OK,SlangResult checking=SLANG_OK) {
    current_calls=wait_calls=0;tick=100;
    return retire_frame_fence(1,target,41,[&](std::uint64_t& value) {
      value=current_calls++?after:before;return checking;
    },[&] {++wait_calls;return waiting;},[&] {return tick+=10;});
  };
  auto r=run(0,0,0);require(r.success && !current_calls && !wait_calls && r.source_frame==UINT64_MAX,"empty slot queried");
  r=run(7,7,7);require(r.success && current_calls==1 && !wait_calls && !r.waited && r.source_frame==41,
      "already completed slot waited or lost source frame");
  require(r.times[1]==r.times[2] && r.times[2]==r.times[3],"skipped wait fabricated duration");
  r=run(7,5,7);require(r.success && current_calls==2 && wait_calls==1 && r.before==5 && r.after==7 && r.value==7,
      "pending slot completion identity wrong");
  require(r.times==std::array<std::uint64_t,4>{110,120,130,140},"fence stage timestamp order wrong");
  r=run(7,5,7,SLANG_E_TIME_OUT);require(!r.success && r.result==SLANG_E_TIME_OUT && current_calls==1 && r.after==UINT64_MAX,
      "failed wait accepted or postcheck invented");
  r=run(7,5,6);require(!r.success && r.result==SLANG_FAIL,"stale completion reused slot");
  r=run(7,UINT64_MAX,0);require(!r.success && !wait_calls,"device loss sentinel accepted");
  r=run(7,0,0,SLANG_OK,SLANG_FAIL);require(!r.success && !wait_calls,"precheck failure ignored");
}
void trace_cases(const std::filesystem::path& directory) {
  _putenv_s("OCTARYN_CLIENT_FRAME_CPU_TRACE","1");
  const auto path=(directory/"retirement.csv").string();_putenv_s("OCTARYN_CLIENT_FRAME_CPU_TRACE_PATH",path.c_str());
  FrameCpuProfile profile;require(profile.initialize(),"trace initialize");
  {
    FrameCpuTrace trace(profile,42,"world");trace.begin("window_state");trace.begin("frame_fence");
    unsigned calls{};
    trace.fence(retire_frame_fence(0,9,40,[&](std::uint64_t& value) {value=calls++?9:8;return SLANG_OK;},
        [] {return SLANG_OK;},[] {return FrameCpuTrace::now();}));
    trace.begin("lighting_query_resolve");trace.begin("renderer_local_cleanup");
    require(trace.complete(),"complete record did not publish");
  }
  {FrameCpuTrace trace(profile,43,"menu");trace.begin("menu_resize");require(trace.complete("skipped"),"skip failed");}
  {FrameCpuTrace trace(profile,43,"world");trace.begin("surface_resize");trace.finish("abandoned");require(trace.complete(),"abandoned failed");}
  require(profile.close(),"trace close");
  const auto overflow_path=(directory/"trace-overflow.csv").string();_putenv_s("OCTARYN_CLIENT_FRAME_CPU_TRACE_PATH",overflow_path.c_str());
  FrameCpuProfile saturated;require(saturated.initialize(),"overflow trace initialize");saturated.begin(1,1,0,"test");
  for(unsigned i=0;i<129;++i)saturated.interval("stage",i,i+1,0);
  saturated.finish(130,"complete");require(!saturated.healthy() && !saturated.close(),"interval overflow silently accepted");
  _putenv_s("OCTARYN_CLIENT_FRAME_CPU_TRACE","0");FrameCpuProfile disabled;require(disabled.initialize() && !disabled.enabled(),"production trace not disabled");
}
}
int main(int argc,char** argv) try {
  require(argc==2,"fresh output path required");const std::filesystem::path directory(argv[1]);
  require(!std::filesystem::exists(directory),"output must be fresh");std::filesystem::create_directories(directory);
  writer_cases(directory);fence_cases();trace_cases(directory);
  std::cout<<"frame_retirement_probe passed=1 ordered_rows=10000 queue_overflow=invalid write_error=invalid close_error=invalid fence_cases=7 gpu_runtime=0\n";
  return 0;
} catch(const std::exception& error) {std::cerr<<"frame_retirement_probe failed="<<error.what()<<'\n';return 1;}
