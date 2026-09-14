#include "Probe.h"
#include <cmath>
#include <cstring>

namespace mesh_probe {
namespace {
void same(const Image& a,const Image& b) {
  require(a.depth==b.depth,"batch changed exact depth");
  for(unsigned i=0;i<4;++i)require(a.mrt[i]==b.mrt[i],"batch changed exact MRT bytes");
}
}
void batch_cases(Fixture& f) {
  auto& r=f.renderer;
  require(world_batch_initialize(r),"batch initialization failed");
  require(r.batch && r.batch->available,"required batch capabilities unavailable");
  auto& batch=*r.batch;batch.required=true;
  r.sources.clear();r.columns.clear();
  auto left=column(),right=column(1,0);
  for(int z=6;z<25;++z)for(int y=2;y<13;++y)for(int x=2;x<25;++x)put(left,x,y,z,1);
  for(int z=3;z<20;++z)for(int y=3;y<22;++y)for(int x=4;x<29;++x)put(right,x,y,z,5);
  // Each column contains multiple opaque patches and multiple authored sprite
  // patches. Sprite patchBase and all but the first draw's firstInstance differ
  // from zero, independently exercising record and local-instance indexing.
  put(left,26,3,20,25);put(right,1,3,24,10);
  const auto a=f.mesh(left),b=f.mesh(right);
  for(const auto* c:{&a.gpu,&b.gpu})
    require(c->patch_counts[0]>1 && c->patch_counts[1]>1,"batch fixture needs multiple instances in both passes");
  r.columns.emplace(std::make_pair(0,0),a.gpu);r.columns.emplace(std::make_pair(1,0),b.gpu);
  const auto hardware_limit=batch.max_draws;
  for(unsigned view=0;view<2;++view)for(unsigned mode=0;mode<3;++mode) {
    const float z=view==0?83.f:-60.f,dz=13-z;
    const WorldCamera camera{32,28,z,std::atan2(0.f,-dz),std::atan2(-18.f,std::abs(dz)),1.05f};
    batch.enabled=false;
    const auto reference=f.render(a.gpu,camera,mode!=0,mode==2,true);
    require(batch.submitted_commands==4,"legacy fixture must submit four actual commands");
    batch.enabled=true;batch.max_draws=hardware_limit;
    const auto actual=f.render(a.gpu,camera,mode!=0,mode==2,true);same(reference,actual);
    require(batch.prepared && batch.submitted_commands==2 && batch.submitted_columns==4,
        "batch fixture did not use two actual multi-draw commands");
    require(batch.cpu_records.size()==4 && batch.first[1]==2,"batch pass record offset");
    for(unsigned i=0;i<4;++i)require(batch.cpu_arguments[i].first_instance==i &&
        batch.cpu_arguments[i].instances>1,"batch nonzero base instance contract");
    require(batch.cpu_records[2].patch_base>0 && batch.cpu_records[3].patch_base>0,"sprite patch base contract");
    batch.max_draws=1;
    same(reference,f.render(a.gpu,camera,mode!=0,mode==2,true));
    require(batch.submitted_commands==4,"indirect limit splitting must preserve global record indices");
    std::printf("world_batch_parity view=%u mode=%u columns=2 legacy_commands=4 batch_commands=2 split_commands=4 mrt=byte_identical depth=identical\n",view,mode);
  }
  batch.max_draws=hardware_limit;
  // Fixture render waits for completion, permitting explicit slot recycling.
  // Keep only raw owner identities here: the test must not itself extend their
  // lifetimes and mask a missing per-slot retained owner.
  auto& first=batch.frames[0];auto& second=batch.frames[1];
  require(batch.active_slot==0 && first.records!=second.records && first.arguments!=second.arguments &&
      first.mapped_records!=second.mapped_records && first.mapped_arguments!=second.mapped_arguments,
      "batch frame slots share writable upload storage");
  require(batch.gpu_bytes()==2ull*WorldBatchMaxColumns*2*(sizeof(WorldBatchRecord)+sizeof(WorldBatchArguments)),
      "batch frame upload accounting");
  std::vector<unsigned char> saved_records(batch.cpu_records.size()*sizeof(WorldBatchRecord));
  std::vector<unsigned char> saved_arguments(batch.cpu_arguments.size()*sizeof(WorldBatchArguments));
  std::memcpy(saved_records.data(),first.mapped_records,saved_records.size());
  std::memcpy(saved_arguments.data(),first.mapped_arguments,saved_arguments.size());
  std::vector<rhi::IBuffer*> saved_owners;
  for(const auto& owner:first.retained)saved_owners.push_back(owner.get());
  require(!saved_owners.empty(),"batch slot ownership fixture is empty");
  const auto first_unchanged=[&] {
    require(std::memcmp(saved_records.data(),first.mapped_records,saved_records.size())==0 &&
        std::memcmp(saved_arguments.data(),first.mapped_arguments,saved_arguments.size())==0,
        "preparing another frame overwrote pending batch data");
    require(first.retained.size()==saved_owners.size(),"another frame retired pending mesh owners");
    for(std::size_t i=0;i<saved_owners.size();++i)
      require(first.retained[i].get()==saved_owners[i],"another frame replaced pending mesh owners");
  };
  require(world_batch_begin_frame(r,1),"batch slot one selection");
  first_unchanged();
  // Replacement and eviction occur after completed frames. The next preparation
  // must rebuild records from live columns and retire previous descriptor owners.
  r.columns.erase({1,0});r.columns.insert_or_assign({0,0},b.gpu);
  const WorldCamera camera{48,28,83,0,-.3f,1.05f};
  batch.enabled=false;const auto replaced=f.render(b.gpu,camera,true,true,true);
  batch.enabled=true;same(replaced,f.render(b.gpu,camera,true,true,true));
  require(batch.cpu_records.size()==2 && batch.frame().retained.size()==4,"batch replacement retained stale columns");
  first_unchanged();
  r.columns.clear();
  const auto empty=f.render(b.gpu,camera,false,false,true);
  require(batch.cpu_records.empty() && batch.frame().retained.empty() && batch.submitted_commands==0,"empty batch retained stale work");
  for(const auto depth:empty.depth)require(depth==1,"empty batch rendered stale geometry");
  first_unchanged();
  require(world_batch_begin_frame(r,0) && first.retained.empty(),"completed slot retained stale mesh owners");
  same(empty,f.render(b.gpu,camera,false,false,true));
  std::puts("world_batch_frame_slots=passed slots=2 independent_uploads=1 pending_data_preserved=1 owners_preserved=1 recycled_empty=1");
  seam_cases(f);
  require(batch.prepared && batch.enabled,"seam coverage must execute the new batch path");
  batch.enabled=false;
  std::puts("world_batch_parity=passed exact_topology=1 nonzero_record=1 nonzero_patch_base=1 instances_per_draw_gt1=1 split_limit=1 replacement=1 empty=1 seams=1");
}
}
