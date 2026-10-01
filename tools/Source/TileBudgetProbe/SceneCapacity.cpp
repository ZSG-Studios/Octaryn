#include "WorldRayCapacity.h"
#include "ItemHistory.h"
#include "ItemHistoryMemory.h"
#include "ItemHistoryState.h"
#include <cassert>
#include <cstdio>
#include <memory_resource>
#include <memory>
#include <stdexcept>

using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::world_ray;
namespace {
struct CountingMemory final:std::pmr::memory_resource {
  std::size_t allocations{},live{},peak{};
  void* do_allocate(std::size_t bytes,std::size_t alignment) override {
    ++allocations;live+=bytes;peak=std::max(peak,live);
    return std::pmr::new_delete_resource()->allocate(bytes,alignment);
  }
  void do_deallocate(void* pointer,std::size_t bytes,std::size_t alignment) override {
    live-=bytes;std::pmr::new_delete_resource()->deallocate(pointer,bytes,alignment);
  }
  bool do_is_equal(const std::pmr::memory_resource& other)const noexcept override {return this==&other;}
};
void history_test(unsigned capacity) {
  CountingMemory memory;
  {
    ItemHistoryMemory pool(&memory);ItemHistory history(&pool);
    assert(prewarm_item_history(history,capacity,MaximumItemCapacity));
    const auto allocations=memory.allocations;
    for(unsigned frame=1;frame<=8;++frame) {
      const std::uint64_t base=std::uint64_t(frame)*MaximumItemCapacity;
      for(unsigned i=0;i<capacity;++i)history.insert_or_assign(base+i,ItemPreviousPose{frame,frame,frame,frame,{1,2,3}});
      std::erase_if(history,[&](const auto& item){return item.second.frame!=frame;});
      assert(history.size()==capacity && history.at(base).generation==frame);
      if(memory.allocations!=allocations) {
        std::printf("history_pool_growth capacity=%u frame=%u before=%zu after=%zu buckets=%zu\n",
            capacity,frame,allocations,memory.allocations,history.bucket_count());std::fflush(stdout);
      }
      assert(memory.allocations==allocations);
    }
    assert(!prewarm_item_history(history,capacity,MaximumItemCapacity));
    assert(history.size()==capacity);
  }
  assert(memory.live==0);
  std::printf("item_history_capacity=%u generations=2 repeated_replacement_allocations=0 peak_upstream_bytes=%zu\n",capacity,memory.peak);
}
void ring_test() {
  struct Scene {unsigned revision{};};
  std::array<std::shared_ptr<Scene>,SceneSnapshotCount> pool;
  for(auto& scene:pool)scene=std::make_shared<Scene>();
  std::array<std::shared_ptr<Scene>,SceneFrameCount> frames;std::shared_ptr<Scene> current;
  for(unsigned revision=1;revision<=4096;++revision) {
    const auto slot=revision%SceneFrameCount;
    // This release models only a frame whose fence has completed.
    frames[slot].reset();
    if(revision%7==0) {frames[slot]=current;continue;}
    auto next=exclusive_snapshot(pool);assert(next && next!=current);
    for(const auto& frame:frames)assert(next!=frame);
    const auto previous=current?current->revision:0;
    next->revision=revision;
    if(current)assert(current->revision==previous);
    current=next;frames[slot]=next;
  }
  frames={};current.reset();
  std::array<std::shared_ptr<Scene>,SceneSnapshotCount> held=pool;
  assert(!exclusive_snapshot(pool));
  held[1].reset();assert(exclusive_snapshot(pool)==pool[1]);
}
void memory_bound_test() {
  CountingMemory memory;
  {
    ItemHistoryMemory pool(&memory);bool rejected=false;
    try {(void)pool.allocate(8*1024*1024,8);}catch(const std::bad_alloc&){rejected=true;}
    assert(rejected && memory.allocations==0);
    std::array<void*,7> blocks{};
    for(unsigned i=0;i<blocks.size();++i)blocks[i]=pool.allocate(32+i,64);
    for(auto* block:blocks)assert(reinterpret_cast<std::uintptr_t>(block)%64==0);
    rejected=false;
    try {(void)pool.allocate(512,64);}catch(const std::bad_alloc&){rejected=true;}
    assert(rejected); // The rejected oversized request consumed one bounded size class.
    for(unsigned i=0;i<blocks.size();++i)pool.deallocate(blocks[i],32+i,64);
  }
  assert(memory.live==0);
}
void history_owner_test() {
  CountingMemory memory;
  {
    auto current=std::make_unique<ItemHistoryState>(&memory);
    assert(prewarm_item_history(current->poses,1000,MaximumItemCapacity));
    current->poses.emplace(42,ItemPreviousPose{7,7,7,7,{1,2,3}});
    const auto* original=current.get();const auto retained=memory.live;
    {
      auto discarded=std::make_unique<ItemHistoryState>(&memory);
      assert(prewarm_item_history(discarded->poses,1000,MaximumItemCapacity));
      discarded->poses.emplace(99,ItemPreviousPose{9,9,9,9,{3,2,1}});
      assert(memory.live>retained);
    }
    assert(current.get()==original && current->poses.at(42).generation==7 && memory.live==retained);
    ItemHistoryMemory* allocator{};
    {
      auto candidate=std::make_unique<ItemHistoryState>(&memory);
      assert(prewarm_item_history(candidate->poses,1000,MaximumItemCapacity));
      candidate->poses.emplace(77,ItemPreviousPose{11,11,11,11,{4,5,6}});
      allocator=&candidate->memory;
      current=std::move(candidate);
      assert(!candidate && current.get()!=original);
    }
    assert(current->poses.get_allocator().resource()==allocator && current->poses.at(77).position[2]==6);
    const auto allocations=memory.allocations;
    for(unsigned frame=12;frame<20;++frame) {
      const std::uint64_t first=std::uint64_t(frame)*1000;
      for(unsigned i=0;i<1000;++i)current->poses.insert_or_assign(first+i,ItemPreviousPose{frame,frame,frame,frame,{1,2,3}});
      std::erase_if(current->poses,[&](const auto& entry){return entry.second.frame!=frame;});
      assert(current->poses.size()==1000 && current->poses.at(first).generation==frame);
      assert(memory.allocations==allocations && current->poses.get_allocator().resource()==allocator);
    }
  }
  assert(memory.live==0);
  std::puts("item_history_ownership discarded_candidate_preserved=1 committed_allocator_stable=1 generations=8 new_allocations=0 final_live_bytes=0");
}
}
int main() {
  const auto normal=capacity_plan(512,1000,4,64,64,32,100001,1025);
  assert(normal && normal->instances==1513 && normal->instance_bytes==131072);
  assert(normal->scratch_bytes==2048 && normal->map_record_bytes==65536);
  assert(normal->tlas_bytes==131072 && normal->total_bytes==856160);
  assert(capacity_plan(512,10000,4,64,64,32,1000001,1025));
  assert(!capacity_plan(512,10001,4,64,64,32,100001,1025));
  assert(!capacity_plan(0,1000,4,64,64,32,100001,1025));
  assert(!capacity_plan(512,1000,4,0,64,32,100001,1025));
  assert(!capacity_plan(512,1000,4,64,64,32,UINT64_MAX,1025));
  assert(!capacity_plan(512,1000,4,64,64,32,MaximumPrewarmBytes,1025));
  assert(!capacity_bytes(UINT64_MAX,64));
  assert(item_prewarm_capacity(nullptr)==1000 && item_prewarm_capacity("10000")==10000);
  for(const auto* invalid:{"0","1001","10001","1000garbage","","-1000"})assert(!item_prewarm_capacity(invalid));
  ring_test();history_test(1000);history_test(10000);memory_bound_test();history_owner_test();
  std::puts("scene_capacity_tests passed=1 ring_rotations=4096 immutable_inflight=1 growth_bound=1");
}
