#include "Probe.h"
#include "core/resource-retirement.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace retirement_probe {
namespace {
using namespace std::chrono_literals;
struct State {
  std::mutex mutex;
  std::condition_variable changed;
  std::vector<unsigned> started,finished;
  std::vector<std::thread::id> threads;
  std::function<void(unsigned)> callback;
  std::atomic<unsigned> destroyed{},late_destroy{};
  std::atomic<bool> owner_alive{true};
  bool blocked{},released{};
  void open() {
    {std::lock_guard lock(mutex);released=true;}
    changed.notify_all();
  }
  bool wait_started(std::size_t count) {
    std::unique_lock lock(mutex);
    return changed.wait_for(lock,2s,[&]{return started.size()>=count;});
  }
};
struct Record {
  State* state;
  unsigned id;
  ~Record() {
    if(!state->owner_alive.load())++state->late_destroy;
    ++state->destroyed;
  }
};
using Worker=rhi::ResourceRetirement<Record>;
void destroy(Record* record) {
  auto& state=*record->state;const unsigned id=record->id;
  {
    std::lock_guard lock(state.mutex);
    state.started.push_back(id);state.threads.push_back(std::this_thread::get_id());
  }
  state.changed.notify_all();
  if(state.callback)state.callback(id);
  if(state.blocked && id==0) {
    std::unique_lock lock(state.mutex);
    state.changed.wait(lock,[&]{return state.released;});
  }
  delete record;
  {
    std::lock_guard lock(state.mutex);state.finished.push_back(id);
  }
  state.changed.notify_all();
}
struct ReleaseGate {
  State& state;
  ~ReleaseGate(){state.open();}
};
bool enqueue(Worker& worker,State& state,unsigned id,std::uint64_t epoch,std::uint64_t bytes) {
  auto record=std::make_unique<Record>(&state,id);
  if(!worker.enqueue(record.get(),epoch,bytes))return false;
  (void)record.release();return true;
}
void capacity_and_fence() {
  State state;state.blocked=true;
  Worker worker(destroy,4);ReleaseGate release{state};
  for(unsigned i=0;i<4;++i)require(enqueue(worker,state,i,10,(i+1)*10),"bounded admission rejected free slot");
  worker.advance(9);
  auto info=worker.info();
  require(info.pendingCount==4 && info.pendingBufferBytes==100 && info.activeCount==0,
      "unfinished fence entered destruction or lost ownership");
  worker.advance(10);require(state.wait_started(1),"completed fence did not wake worker");
  info=worker.info();
  require(info.pendingCount==4 && info.activeCount==1 && info.pendingBufferBytes==100,
      "active resource omitted from queue bound or byte accounting");
  auto rejected=std::make_unique<Record>(&state,9);
  require(!worker.enqueue(rejected.get(),10,99) && rejected->id==9,"full admission consumed producer ownership");
  require(!worker.enqueue(nullptr,10,0),"null resource accepted");
  require(worker.info().peakCount==4 && worker.info().peakBufferBytes==100,"retirement peak accounting differs");
  state.open();worker.finish();
  info=worker.info();
  require(info.pendingCount==0 && info.pendingBufferBytes==0 && info.activeCount==0,"finish left owned resources");
  require(state.finished==std::vector<unsigned>({0,1,2,3}),"eligible FIFO destruction reordered or duplicated");
  require(state.destroyed==4 && state.late_destroy==0,"worker ownership did not release exactly once");
  for(const auto thread:state.threads)require(thread!=std::this_thread::get_id(),"resource destroyed on producer thread");
  require(!worker.enqueue(rejected.get(),10,99),"closed worker accepted producer ownership");
}
void callbacks_outside_lock() {
  State state;Worker worker(destroy,2);
  std::atomic<bool> queried{},nested_admitted{};
  state.callback=[&](unsigned id) {
    if(id==0) {
      const auto info=worker.info();queried=info.activeCount==1 && info.pendingCount==1;
      nested_admitted=enqueue(worker,state,1,0,20);
    }
  };
  require(enqueue(worker,state,0,0,10),"callback fixture admission failed");
  require(state.wait_started(2),"destructor callback could not query/enqueue outside worker lock");
  worker.finish();
  require(queried && nested_admitted && state.finished==std::vector<unsigned>({0,1}),
      "reentrant destructor ownership was lost");
}
void finish_waits_for_active() {
  State state;state.blocked=true;
  Worker worker(destroy,2);ReleaseGate release{state};
  require(enqueue(worker,state,0,7,32) && enqueue(worker,state,1,7,64),"finish fixture admission");
  worker.advance(7);require(state.wait_started(1),"finish fixture worker never started");
  std::atomic<bool> finishing{},returned{};
  std::thread closer([&]{finishing=true;worker.finish();returned=true;});
  while(!finishing.load())std::this_thread::yield();
  require(!returned && state.owner_alive && worker.info().pendingCount==2,"finish abandoned active destructor");
  state.open();closer.join();state.owner_alive=false;
  require(returned && state.destroyed==2 && state.late_destroy==0,"device owner outlived unfinished deletion");
  require(worker.info().pendingCount==0,"finish returned before queued resource destruction");
}
void capacity_wake() {
  State state;state.blocked=true;
  Worker worker(destroy,1);ReleaseGate release{state};
  require(enqueue(worker,state,0,0,16),"capacity wake admission");
  require(state.wait_started(1),"capacity wake worker did not start");
  std::atomic<bool> waiting{},returned{};
  std::thread waiter([&]{waiting=true;worker.waitForCapacity();returned=true;});
  while(!waiting.load())std::this_thread::yield();
  require(!returned,"capacity wait ignored active destructor");
  state.open();waiter.join();worker.finish();
  require(returned && state.destroyed==1,"capacity waiter missed completion wake");
}
void concurrent_producers() {
  State state;Worker worker(destroy,16);worker.advance(100);
  constexpr unsigned Producers=4,PerProducer=512;
  std::vector<std::thread> producers;
  std::atomic<unsigned> accepted{},retries{};
  for(unsigned p=0;p<Producers;++p)producers.emplace_back([&,p] {
    for(unsigned i=0;i<PerProducer;++i) {
      auto record=std::make_unique<Record>(&state,p*PerProducer+i);
      while(!worker.enqueue(record.get(),100,32)) {++retries;std::this_thread::yield();}
      (void)record.release();++accepted;
    }
  });
  for(auto& producer:producers)producer.join();
  worker.finish();
  std::vector<bool> seen(Producers*PerProducer);
  for(const unsigned id:state.finished) {
    require(id<seen.size() && !seen[id],"concurrent producer deletion duplicated identity");seen[id]=true;
  }
  const auto info=worker.info();
  require(accepted==seen.size() && state.destroyed==seen.size() && state.finished.size()==seen.size(),
      "concurrent producer retirement lost resource ownership");
  require(info.peakCount<=16 && info.peakBufferBytes<=16*32 && info.pendingCount==0,
      "concurrent producers exceeded count/byte capacity");
  std::printf("resource_retirement_producers=passed producers=%u resources=%u retries=%u\n",
      Producers,accepted.load(),retries.load());
}
void repeated_idle_shutdown() {
  State state;
  for(unsigned i=0;i<128;++i) {Worker worker(destroy,1);worker.finish();worker.finish();}
  require(state.destroyed==0,"idle shutdown invented resource deletion");
}
}
void worker_cases() {
  capacity_and_fence();callbacks_outside_lock();finish_waits_for_active();capacity_wake();
  concurrent_producers();repeated_idle_shutdown();
  std::puts("resource_retirement_worker=passed fence=1 bounded_active=1 callback_outside_lock=1 finish_drain=1 capacity_wake=1 idle_stops=128");
}
}
