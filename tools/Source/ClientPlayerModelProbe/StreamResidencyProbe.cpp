#include "StreamResidency.h"
#include "StreamGenerationOrder.h"
#include <stdexcept>

namespace {
using namespace octaryn::client::world_presentation;
void require(bool value,const char* reason) {if(!value) throw std::runtime_error(reason);}
StreamColumn column(int x,int z,std::uint64_t revision=7) {
  StreamColumn result;result.x=x;result.z=z;result.revision=revision;
  result.blocks={static_cast<std::uint16_t>(revision)};return result;
}
void finish(StreamResidency& state,StreamColumn value) {
  auto query=std::make_shared<const StreamColumn>(value);
  require(state.retain(std::move(value),std::move(query)),"wanted generation must publish");
}
StreamColumn consume(StreamResidency& state) {
  StreamColumn result;require(state.deliver(result),"wanted ready column must deliver");return result;
}
void prune(StreamResidency& state) {
  StreamResidency::RetiredPayloads retired;
  state.collect_retired(retired);
  retired.clear();
}
void check_detached_retirement() {
  int destroyed=0;
  StreamResidency state;state.change_window(0,0,32);
  StreamResidency::RetiredPayloads retired;
  const auto columns_capacity=retired.columns.capacity(),ready_capacity=retired.ready.capacity();
  for(int z=-32;z<=32;++z) for(int x=-32;x<=32;++x) {
    auto value=column(x,z);
    auto query=std::shared_ptr<const StreamColumn>(new StreamColumn(value),[&](const auto* ptr) {
      ++destroyed;delete ptr;
    });
    require(state.retain(std::move(value),std::move(query)),"full window publication must fit bounded mailbox");
    consume(state);
  }
  state.collect_retired(retired);
  require(retired.empty() && !state.spatial_prune_pending,"unchanged full window requires no retired owners");
  state.change_window(0,0,1);
  state.collect_retired(retired);
  require(destroyed==0 && retired.columns.size()==4216 && state.query_columns.size()==9,
      "shrinking must detach all retired map nodes without running payload deleters under collection lock");
  require(state.query(0,0) && !state.query(2,0),"detached cleanup must preserve retained query visibility");
  state.change_window(0,0,32);
  require(state.needs_generation(2,0,7),"regrowth during detached destruction must still regenerate evicted columns");
  retired.clear();
  require(destroyed==4216,"detached worker cleanup must release every removed query owner");
  state.collect_retired(retired);
  require(retired.empty(),"regrowth must not retire still-resident overlapping queries");
  for(int i=0;i<64;++i) state.collect_retired(retired);
  require(retired.columns.capacity()==columns_capacity && retired.ready.capacity()==ready_capacity,
      "repeated cleanup must retain its bounded scratch capacity");
  finish(state,column(32,0));finish(state,column(32,1));
  std::weak_ptr<const StreamColumn> ready_query=state.ready.front().query;
  state.change_window(0,0,1);state.collect_retired(retired);
  require(state.ready.empty() && retired.ready.size()==2 && !ready_query.expired(),
      "discarded mailbox payloads must remain alive until detached cleanup");
  retired.clear();require(ready_query.expired(),"detached mailbox cleanup must release its paired query");
}
void check_delivery_queries() {
  StreamResidency state;state.change_window(0,0,4);
  auto initial=column(0,0,1);const auto* payload=initial.blocks.storage_identity();
  finish(state,std::move(initial));
  const auto* immutable=state.ready.front().query.get();
  require(!state.query(0,0),"generated but undelivered terrain must not be queryable");
  auto delivered=consume(state);
  require(delivered.blocks.storage_identity()==payload && state.query(0,0).get()==immutable &&
      delivered.blocks.storage_identity()==state.query(0,0)->blocks.storage_identity(),
      "delivery must move payload and publish paired immutable query without another block copy");
  std::weak_ptr<const StreamColumn> first=state.query(0,0);
  finish(state,column(0,0,2));finish(state,column(0,0,3));
  require(state.query(0,0)->revision==1 && state.query(0,0)->blocks[0]==1,
      "queued tail completion must not replace delivered query");
  auto extra=column(0,0,4);auto extra_query=std::make_shared<const StreamColumn>(extra);
  require(!state.retain(std::move(extra),std::move(extra_query)) && state.ready.size()==2,
      "ready delivery queue remains bounded");
  delivered=consume(state);
  require(delivered.revision==2 && state.query(0,0)->revision==2 && state.query(0,0)->blocks[0]==2,
      "first delivery must publish its exact revision, not newest queued revision");
  std::weak_ptr<const StreamColumn> second=state.query(0,0);
  delivered=consume(state);
  require(delivered.revision==3 && state.query(0,0)->blocks[0]==3 && !first.expired() && !second.expired(),
      "superseded queries must remain worker-owned after frame delivery");
  finish(state,column(0,0,4));
  require(!state.deliver(delivered) && delivered.revision==3 && state.query(0,0)->revision==3 && state.ready.size()==1,
      "full retirement slots must backpressure without losing or prematurely publishing ready revision");
  prune(state);
  require(first.expired() && second.expired(),"worker cleanup must release both retired query owners");
  delivered=consume(state);require(delivered.revision==4 && state.query(0,0)->revision==4,"delivery resumes after worker cleanup");

  prune(state);finish(state,column(-4,0,5));
  std::weak_ptr<const StreamColumn> unwanted=state.ready.front().query;
  state.change_window(1,0,4);
  require(!state.deliver(delivered) && !unwanted.expired() && state.ready.size()==1,
      "frame poll must leave unwanted payload destruction to worker");
  prune(state);require(unwanted.expired() && state.ready.empty(),"worker prunes unwanted queue payload");
}

void check_large_window() {
  StreamResidency state;
  state.change_window(-3,5,32);
  require(state.radius==32 && state.wanted(-35,-27) && state.wanted(29,37) && !state.wanted(30,37),
      "radius32 must retain the exact signed65x65 window");
  finish(state,column(-35,-27,9));consume(state);
  finish(state,column(29,37,10));consume(state);
  state.change_window(-3,5,4);
  require(!state.query(-35,-27) && !state.query(29,37),"shrinking must hide distant delivered queries immediately");
  state.change_window(-3,5,32);
  require(!state.query(-35,-27) && state.needs_generation(-35,-27,9),
      "regrowing before worker maintenance must regenerate distant retired columns");
  prune(state);

  StreamSnapshot snapshot;
  for(int z=-27;z<=37;++z) for(int x=-35;x<=29;++x) snapshot.columns.push_back({x,z,1,{}});
  StreamGenerationOrder order;order.reset(snapshot,-3,5,32);
  int count=0;std::tuple<std::int64_t,std::int64_t> previous{-1,-1};
  while(const auto* selected=order.next(state)) {
    const auto dx=std::abs(static_cast<std::int64_t>(selected->x)+3);
    const auto dz=std::abs(static_cast<std::int64_t>(selected->z)-5);
    const auto priority=std::tuple{std::max(dx,dz),dx+dz};
    require(priority>=previous,"worker candidates must preserve center-first ring ordering");
    previous=priority;state.completed[{selected->x,selected->z}]=selected->revision;
    order.consumed();++count;
  }
  require(count==4225,"large window schedule must include everycolumn exactlyonce");
  const auto revision=state.window_revision;
  state.change_window(-2,5,32);state.change_window(-3,5,32);
  require(state.window_revision==revision+2,"rapidreturn must invalidate consumed worker candidate plan");
  order.reset(snapshot,-3,5,32);
  const auto* edge=order.next(state);
  require(edge && edge->x==-35,"returning window must schedule GPU-retired edge even with unchanged snapshot");
}
}
void check_stream_residency() {
  check_detached_retirement();
  check_delivery_queries();
  check_large_window();
  StreamResidency state;state.change_window(0,0,4);
  finish(state,column(-4,0));consume(state); // Delivered to renderer in window A.
  finish(state,column(0,0));consume(state);
  finish(state,column(-4,1)); // A second edge column still awaits delivery.
  auto inflight=column(-4,2); // Worker generation begins, then pauses outside mutex.

  // No worker maintenance runs between A -> B -> A. These calls are the same
  // residency transition used synchronously by actual WorldStream::request.
  state.change_window(1,0,4);
  require(!state.completed.contains({-4,0}) && !state.completed.contains({-4,1}) &&
      !state.wanted(-4,0),"temporary window must invalidate delivered/queued completion metadata immediately");
  require(state.query_columns.contains({-4,0}) && state.ready.size()==1,
      "frame request must defer large payload destruction to worker");
  require(!state.query(-4,0),"departed column must immediately stop serving queries");
  state.change_window(0,0,4);
  require(!state.query(-4,0),"returning before worker maintenance must not reveal GPU-retired query");
  require(state.needs_generation(-4,0,7) && state.needs_generation(-4,1,7),
      "center reversal must regenerate previously delivered and queued retired edges");
  require(!state.needs_generation(0,0,7),"overlapping unchanged resident must not rebuild");
  require(state.wanted(state.ready.front().column.x,state.ready.front().column.z),"queued payload may deliver after return");
  consume(state); // Consume that queued valid edge after the reversal.
  finish(state,std::move(inflight)); // Still wanted after reversal: do not cancel by request serial.
  require(!state.needs_generation(-4,2,7) && state.ready.size()==1,
      "in-flight column returning to wanted window must deliver normally");
  consume(state);finish(state,column(-4,0));
  require(state.ready.front().column.x==-4 && !state.needs_generation(-4,0,7),
      "retired unchanged revision must become deliverable again");
  require(!state.change_window(0,0,4) && state.ready.size()==1,
      "idempotent request must preserve ready delivery");
  require(state.needs_generation(-4,0,8),"new authoritative edit revision must regenerate");
  consume(state);require(state.query(-4,0)!=nullptr,"redelivery restores query visibility after GPU residency returns");

  state.change_window(1,0,4);
  prune(state);
  require(!state.query_columns.contains({-4,0}),"worker must eventually release retired payloads");
  auto outside=column(-4,0,8);auto query=std::make_shared<const StreamColumn>(outside);
  require(!state.retain(std::move(outside),std::move(query)) && state.ready.empty() &&
      !state.completed.contains({-4,0}),"in-flight completion outside latest window must be discarded");
  state.change_window(0,0,4);finish(state,column(4,4));consume(state);
  state.change_window(0,0,1);state.change_window(0,0,4);
  require(state.needs_generation(4,4,7),"shrink/regrow must also invalidate retired edges");
  state.change_window(2000000,-2000000,32);
  require(state.x==1000000 && state.z==-1000000 && state.radius==32,"supported residency bounds");
  state.change_window(0,0,128);require(state.radius==32,"requests above restoredmaximum remain bounded");
  state.change_window(0,0,0);require(state.radius==1,"minimum residency radius");
}
