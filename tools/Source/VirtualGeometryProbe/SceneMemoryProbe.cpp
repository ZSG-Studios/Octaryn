#include "SceneMemoryLedger.h"
#include <atomic>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace octaryn::client::rendering::virtual_geometry;
namespace {
void check(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
void retained_generations() {
  auto ledger=std::make_shared<SceneMemoryLedger>(1024);
  auto pages=ledger->reserve(400,SceneMemoryDomain::Pages);
  auto old=ledger->reserve(300,SceneMemoryDomain::RayGeometry);
  auto submitted_frame=old;old->phase(SceneMemoryPhase::Retired);old.reset();
  check(!ledger->reserve(325,SceneMemoryDomain::RayGeometry,SceneMemoryPhase::Pending),
      "a retained frame was removed from candidate admission");
  auto candidate=ledger->reserve(324,SceneMemoryDomain::RayGeometry,SceneMemoryPhase::Pending);
  check(candidate && ledger->stats().used==1024,"exact complete candidate boundary failed");
  check(!candidate->resize(325) && candidate->bytes()==324 && ledger->stats().used==1024,
      "failed growth changed existing allocation credit");
  candidate->phase(SceneMemoryPhase::Compacting);
  check(ledger->stats().phase_bytes[unsigned(SceneMemoryPhase::Compacting)]==324,"compaction charge missing");
  auto allocation=candidate->split(100,SceneMemoryPhase::Resident);
  check(allocation && candidate->bytes()==224 && ledger->stats().used==1024 && !candidate->split(225,SceneMemoryPhase::Resident),
      "atomic credit transfer changed peak admission or exceeded reserved capacity");
  allocation.reset();check(ledger->stats().used==924,"transferred allocation release failed");
  candidate.reset();check(ledger->stats().used==700,"cancel leaked or released published capacity");
  submitted_frame.reset();check(ledger->stats().used==400,"completed consumer retained charge");
  check(ledger->stats().peak==1024,"peak charge was lost after retirement");
  auto held=pages;ledger.reset();pages.reset();
  check(held->bytes()==400 && held->resize(512),"allocation lifetime depends on scene wrapper lifetime");
}
void overflow_and_ownership() {
  SceneMemoryLedger ledger(std::numeric_limits<std::uint64_t>::max());
  auto full=ledger.reserve(std::numeric_limits<std::uint64_t>::max(),SceneMemoryDomain::Metadata);
  check(full && !ledger.reserve(1,SceneMemoryDomain::Selection),"capacity arithmetic overflowed");
  check(full->resize(1) && ledger.stats().used==1,"large shrink corrupted accounting");
  SceneMemoryLedger other(16);
  check(ledger.owns(full) && !other.owns(full) && !ledger.owns({}),"ledger identity mixed scene lifetimes");
}
void concurrent_admission() {
  SceneMemoryLedger ledger(16);std::atomic<unsigned> held{},arrived{};std::atomic<bool> release{};
  std::vector<std::thread> workers;
  for(unsigned i=0;i<32;++i)workers.emplace_back([&] {
    auto lease=ledger.reserve(1,SceneMemoryDomain::Metadata);
    if(lease)++held;++arrived;
    while(!release.load())std::this_thread::yield();
  });
  while(arrived.load()!=32)std::this_thread::yield();
  const bool bounded=held.load()==16 && ledger.stats().used==16 && ledger.stats().peak==16;
  release=true;for(auto& worker:workers)worker.join();
  check(bounded && ledger.stats().used==0,"concurrent reservations exceeded or leaked the budget");
}
}
int main() {
  try {
    retained_generations();overflow_and_ownership();concurrent_admission();
    std::puts("scene_memory=passed candidate=whole retained=charged compaction=charged cancel=clean concurrency=bounded overflow=rejected");
    return 0;
  } catch(const std::exception& e) {std::fprintf(stderr,"scene_memory=failed reason=%s\n",e.what());return 1;}
}
