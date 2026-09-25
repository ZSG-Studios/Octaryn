// CPU lifecycle test doubles, not a replacement RHI or GPU qualification.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <utility>
#include "RayPrepareDiagnostics.h"

namespace octaryn::client::rendering {
using Coord=std::pair<std::int32_t,std::int32_t>;
struct Mesh {
  unsigned identity{};
  std::array<std::uint32_t,5> pass_counts{};
  int min_y{},height{32};
};
struct Column {
  unsigned identity{};
  struct {std::uint32_t face_count{24};} record;
  bool matches(const Mesh& mesh) const {return identity==mesh.identity;}
};
struct Fence {
  std::int32_t result{};
  std::uint64_t value{};
  unsigned calls{};
  std::int32_t getCurrentValue(std::uint64_t* out) {++calls;*out=value;return result;}
  // No wait method: the production poll must only read the timeline value.
};
struct Timing {
  unsigned resolves{};
  bool valid{true};
  bool resolve(double&,RayPrepareDiagnostics*) {++resolves;return valid;}
};
struct Submission {bool retained{true};void setNull() {retained=false;}};
struct BuildJob {
  std::shared_ptr<Column> pending,refit_source;
  Timing timing;
  Submission submission;
  Coord coordinate{};
  std::uint64_t signal{7};
  bool cancelled{};
};
enum class SceneChangeKind {AccelerationReady};
struct Changes {
  unsigned count{};
  bool minor{};
  void notify_column(int,int,int,int,SceneChangeKind,bool value) {++count;minor=value;}
};
struct WorldRenderer {
  std::map<Coord,Mesh> columns;
  std::uint64_t frames{100};
  Changes scene_changes;
};
struct WorldRayTracing {
  struct State {
    std::array<BuildJob,1> jobs;
    Fence storage;
    Fence* fence{&storage};
    bool bytes_dirty{};
    std::map<Coord,std::shared_ptr<Column>> columns,changed;
    std::map<Coord,std::array<std::uint32_t,5>> built_pass_counts;
    std::uint64_t generation{1};
    struct {double blas_gpu_ms{};std::uint64_t discarded_builds{};} stats;
    bool poll(WorldRenderer&);
  };
};
#include "WorldRayPollUnderTest.h"
}

using namespace octaryn::client::rendering;
void check(bool ok,const char* detail) {
  if(!ok) {std::fprintf(stderr,"FAILED: %s\n",detail);std::exit(1);}
}
struct Fixture {
  WorldRenderer renderer;
  WorldRayTracing::State state;
  Fixture() {
    renderer.columns[{0,0}].identity=1;
    auto& job=state.jobs[0];job.pending=std::make_shared<Column>();job.pending->identity=1;
    job.refit_source=job.pending;state.changed[{0,0}]=job.pending;
  }
};
int main() {
  for(unsigned pass:{0u,4u}) {
    Fixture f;f.state.storage.value=7;
    f.renderer.columns[{0,0}].pass_counts[pass]=24;
    f.state.built_pass_counts[{0,0}]=f.renderer.columns[{0,0}].pass_counts;
    check(f.state.poll(f.renderer),"equal-count replacement publishes");
    check(!f.renderer.scene_changes.minor,"equal-count blockers must invalidate occlusion");
  }
  {
    Fixture f;f.state.storage.value=7;
    f.state.built_pass_counts[{0,0}]={0,8,12,16,0};
    f.renderer.columns[{0,0}].pass_counts={0,12,16,20,0};
    check(f.state.poll(f.renderer)&&f.renderer.scene_changes.minor,"blocker-free replacement stays minor");
  }
  {
    Fixture f;auto& s=f.state;auto& j=s.jobs[0];s.storage.value=6;
    for(unsigned i=0;i<100;++i)check(s.poll(f.renderer),"unfinished fence is not fatal");
    check(j.pending && j.submission.retained && j.refit_source,"unfinished resources retained");
    check(j.timing.resolves==0 && s.columns.empty() && s.generation==1,"no early query or publication");
    s.storage.value=7;check(s.poll(f.renderer),"exact signal publishes");
    check(!j.pending && !j.submission.retained && !j.refit_source,"completed resources retired");
    check(s.columns.size()==1 && s.generation==2 && f.renderer.scene_changes.count==1,"publish exactly once");
    check(s.changed.empty() && j.timing.resolves==1,"replacement complete");
    check(s.poll(f.renderer) && s.generation==2 && j.timing.resolves==1,"idle poll is idempotent");
  }
  {
    Fixture f;f.renderer.columns[{0,0}].identity=2;f.state.storage.value=6;
    check(f.state.poll(f.renderer) && f.state.jobs[0].cancelled,"replacement cancels build");
    check(bool(f.state.jobs[0].pending),"cancelled build retained until its fence");
    f.state.storage.value=8;check(f.state.poll(f.renderer),"later signal completes cancelled job");
    check(f.state.columns.empty() && f.state.generation==1 && f.state.stats.discarded_builds==1,
      "stale mesh never published");
  }
  {
    Fixture f;f.renderer.columns.clear();f.state.storage.value=7;
    check(f.state.poll(f.renderer) && f.state.columns.empty(),"removed coordinate never dereferenced or published");
  }
  for(unsigned mode=0;mode<3;++mode) {
    Fixture f;f.state.storage.value=7;
    if(mode==0)f.state.storage.result=-42;
    if(mode==1)f.state.storage.value=UINT64_MAX;
    if(mode==2)f.state.jobs[0].timing.valid=false;
    check(!f.state.poll(f.renderer),"device/query failure remains fatal");
    check(f.state.jobs[0].pending && f.state.jobs[0].submission.retained && f.state.columns.empty(),
      "failed completion cannot release or publish resources");
  }
  std::puts("world_ray_poll: pending, exact/later completion, replacement/removal, error and device-loss invariants passed");
}
