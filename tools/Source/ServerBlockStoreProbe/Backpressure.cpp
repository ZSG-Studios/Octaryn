#include "BlockChangeQueue.h"
#include "BlockCommandQueue.h"
#include "BlockEditService.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace octaryn::server::world::blocks;
unsigned checks{};
void require(bool value,const char* message) {
  ++checks;
  if(!value) throw std::runtime_error(message);
}
octaryn_host_command command(BlockPosition p,uint16_t block) {
  octaryn_host_command result{};
  result.version=HostCommandVersion;result.size=OCTARYN_HOST_COMMAND_SIZE;
  result.kind=HostCommandSetBlockKind;
  result.a=p.x;result.b=p.y;result.c=p.z;result.d=block;
  return result;
}
uint16_t generated(void*,const octaryn_server_block_position* p) {
  return p->y==0?uint16_t{2}:AirBlock;
}
uint32_t known(void*,uint16_t block) {
  return block==0 || block==2 || block==5 || block==9 || block==14;
}
uint32_t allowed(void*,const octaryn_server_block_edit* edit,uint16_t below) {
  return edit->block!=9 || below==2 || below==5;
}
uint32_t supported(void*,uint16_t block,const octaryn_server_block_position*,uint16_t below) {
  return block!=9 || below==2 || below==5;
}
BlockEditPolicy policy() {return policy_from_abi(generated,known,allowed,supported,nullptr);}
void same_store(const BlockStore& store,const std::vector<BlockEdit>& before) {
  const auto after=store.snapshot();
  require(after.size()==before.size(),"deferred edit changed override presence");
  for(size_t i=0;i<before.size();++i)
    require(after[i].position==before[i].position && after[i].block==before[i].block,
        "deferred edit changed override contents");
}
void fill(BlockChangeQueue& queue,size_t count) {
  for(size_t i=0;i<count;++i)
    require(queue.enqueue({{static_cast<int32_t>(i),-4,-5},5}),"queue fill unexpectedly refused");
}
std::vector<ReplicationChange> drain(BlockChangeQueue& queue) {
  const auto count=static_cast<uint32_t>(queue.pending_count());
  std::vector<ReplicationChange> output(count);uint32_t written=99;
  require(queue.drain(output.data(),count,123,written)==0 && written==count && queue.pending_count()==0,
      "exact-capacity drain must consume entire queue");
  return output;
}
void exact_changes(const std::vector<ReplicationChange>& output,const std::vector<BlockEdit>& edits) {
  require(output.size()==edits.size(),"replication count differs from applied edits");
  for(size_t i=0;i<edits.size();++i) {
    const auto expected=to_replication_change(edits[i],123);
    require(output[i].version==expected.version && output[i].size==expected.size &&
        output[i].change_kind==expected.change_kind && output[i].replication_id==123 &&
        output[i].payload0==expected.payload0 && output[i].payload1==expected.payload1,
        "replication lost, duplicated or reordered exact edit");
  }
}
void ring_limits() {
  require(MaxPendingBlockChanges==8192,"replication bound must be 8192");
  auto queue=std::make_unique<BlockChangeQueue>();
  fill(*queue,8191);
  const std::vector<BlockEdit> pair{{{-1,-2,-3},14},{{-1,-1,-3},0}};
  require(!queue->enqueue_all(pair) && queue->pending_count()==8191,"batch with insufficient room must be atomic");
  require(queue->enqueue(pair[0]) && queue->pending_count()==8192,"last slot accepted");
  require(!queue->can_enqueue(1) && queue->can_enqueue(0) && !queue->can_enqueue(SIZE_MAX),"capacity arithmetic bounded");
  require(!queue->enqueue(pair[1]) && queue->pending_count()==8192,"full ring rejects without overwrite");
  require(queue->enqueue_all({}) && queue->pending_count()==8192,"empty batch succeeds at capacity");
  auto abi_edit=to_abi_block_edit(pair[1]);
  require(octaryn_server_block_change_queue_enqueue(queue.get(),&abi_edit)==-1,"ABI enqueue reports full");
  ReplicationChange sentinel{};sentinel.payload0=987;uint32_t written=99;
  require(queue->drain(&sentinel,1,123,written)==-1 && written==0 && sentinel.payload0==987 &&
      queue->pending_count()==8192,"undersized drain changes neither output nor queue");
  const auto full=drain(*queue);
  for(size_t i=0;i<8191;++i)
    require(full[i].payload0==to_replication_change({{static_cast<int32_t>(i),-4,-5},5},123).payload0,
        "full ring preserves FIFO history");
  exact_changes({full.back()},{pair[0]});
  fill(*queue,8191);drain(*queue); // Advance the retained ring head to force wrap on the next batch.
  require(queue->enqueue_all(pair),"wrapped batch accepted");exact_changes(drain(*queue),pair);
  require(octaryn_server_block_change_queue_enqueue(queue.get(),&abi_edit)==0,"ABI enqueue reports success after recovery");
  exact_changes(drain(*queue),{pair[1]});
}
void edit_and_fifo() {
  const auto rules=policy();BlockStore store;
  const BlockPosition support{-33,0,-65},plant{-33,1,-65},dry{-32,2,-65};
  require(store.set_block({support,5}).changed && store.set_block({plant,9}).changed,"fixture overrides installed");
  const auto original=store.snapshot();auto changes=std::make_unique<BlockChangeQueue>();fill(*changes,8191);
  const auto remove=command(support,AirBlock);
  auto result=apply_block_command_and_enqueue(store,changes.get(),remove,rules);
  require(result.deferred && !result.result.applied && !result.result.changed && result.changes.empty(),
      "two-change cascade must defer with one free slot");same_store(store,original);
  octaryn_server_block_edit output[2]{};uint32_t count=99;
  const auto blocked=octaryn_server_block_edit_service_apply_command_and_enqueue(&store,changes.get(),&remove,
      generated,known,allowed,supported,nullptr,output,2,&count);
  require(!blocked.applied && !blocked.changed && count==0 && changes->pending_count()==8191,
      "direct module ABI refuses cascade before authority mutation");same_store(store,original);
  require(changes->enqueue({{0,4,0},5}),"fill final slot");
  result=apply_block_edit_and_enqueue(store,changes.get(),{support,5},rules);
  require(!result.deferred && result.result.applied && !result.result.changed && result.changes.empty(),
      "exact no-op succeeds while replication full");
  result=apply_block_edit_and_enqueue(store,changes.get(),{dry,65000},rules);
  require(!result.deferred && !result.result.applied,"invalid edit rejected rather than stalled by full queue");
  result=apply_block_edit_and_enqueue(store,changes.get(),{dry,14},rules);
  require(result.deferred && !result.result.applied,"single edit defers at full capacity");same_store(store,original);
  ClientBlockCommandQueue commands;
  const BlockCommandQueuePolicy admission{[](uint16_t){return true;},[](const auto&){return true;}};
  const octaryn_host_command batch[]={command(support,5),command(dry,65000),remove,command(support,2),command(dry,5)};
  size_t rejected{};require(commands.submit(batch,5,admission,rejected)==0,"FIFO fixture accepted");
  std::vector<BlockEdit> observed;unsigned callbacks=0;
  const auto note=[&](const auto&,const BlockEditApplyResult& applied) {
    require(!applied.deferred,"deferred front must not emit completion callback");
    ++callbacks;observed.insert(observed.end(),applied.changes.begin(),applied.changes.end());
  };
  require(commands.drain_apply_and_enqueue(store,changes.get(),rules,note)==1 && callbacks==2 &&
      commands.pending_count()==3 && observed.empty(),"no-op and invalid front retire, blocked cascade retains FIFO tail");
  same_store(store,original);
  require(commands.drain_apply_and_enqueue(store,changes.get(),rules,note)==0 && callbacks==2 &&
      commands.pending_count()==3,"repeated blocked drain preserves front without duplicate callbacks");
  drain(*changes);
  require(commands.drain_apply_and_enqueue(store,changes.get(),rules,note)==3 && callbacks==5 &&
      commands.pending_count()==0,"space recovery resumes exact pending FIFO once");
  const std::vector<BlockEdit> expected{{support,0},{plant,0},{support,2},{dry,5}};
  require(observed.size()==expected.size(),"callbacks include primary and cascade exactly once");
  for(size_t i=0;i<expected.size();++i)
    require(observed[i].position==expected[i].position && observed[i].block==expected[i].block,"callback edit order");
  exact_changes(drain(*changes),expected);
  uint16_t block{};
  require(!store.try_get_block(support,block) && !store.try_get_block(plant,block) && store.get_block(dry)==5,
      "retried restore removes generated-value overrides and retains final edit");
  require(commands.drain_apply_and_enqueue(store,changes.get(),rules,note)==0 && callbacks==5 &&
      changes->pending_count()==0,"completed retry cannot apply twice");
}
}
bool validate_block_backpressure() {
  try {ring_limits();edit_and_fifo();std::printf("block_backpressure=passed checks=%u capacity=8192\n",checks);return true;}
  catch(const std::exception& error) {std::fprintf(stderr,"block_backpressure failed: %s\n",error.what());return false;}
}
