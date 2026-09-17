#include "PredictedColumn.h"
#include <cstdlib>
#include <iostream>

using namespace octaryn::client::world_presentation;
void validate_baseline_coverage(const std::filesystem::path&);
void require(bool value,const char* label) {
  if(!value) {std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}
}
int main(int argc,char** argv) {
  try {
  require(argc==2,"provide tools-owned qualification directory");
  validate_baseline_coverage(argv[1]);
  StreamColumn base;
  base.x=-1;base.z=0;base.min_y=0;base.height=1;base.revision=4953160058118402688ull;base.authoritative_revision=10;
  base.blocks.resize(32*32);base.blocks.fill(1);base.blocks.compact();
  const auto identity=base.blocks.storage_identity();
  PredictedBlocks pending;
  require(pending.add(1,-1,0,0,0),"break admitted");
  auto visible=compose_predicted_column(base,pending);
  require(visible.blocks[31]==0 && base.blocks[31]==1,"immediate break preserves authoritative boundary");
  require(base.blocks.storage_identity()==identity,"base immutable");
  require(pending.resolve(1,false,10),"same revision rejection found");
  visible=compose_predicted_column(base,pending);
  require(visible.blocks[31]==1 && visible.blocks.storage_identity()==identity,"rejection restores mesh source and halo identity");

  require(pending.add(2,-1,0,0,0) && pending.add(3,-1,0,0,7),"rapid same-cell edits");
  base.authoritative_revision=11;pending.cover(-1,0,base.authoritative_revision);
  require(pending.edits().size()==2,"unrelated revision retains unresolved commands");
  std::uint16_t queried{};
  require(pending.query(-1,0,0,queried) && queried==7,"target newest overlay");
  pending.resolve(2,false,11);
  require(compose_predicted_column(base,pending).blocks[31]==7,"reject removes exact command");
  pending.resolve(3,true,13);pending.cover(-1,0,12);
  require(pending.edits().size()==1,"accept waits covering baseline");
  base.blocks[31]=7;base.authoritative_revision=13;pending.cover(-1,0,base.authoritative_revision);
  require(pending.edits().empty() && compose_predicted_column(base,pending).blocks[31]==7,"cover retires accepted overlay");

  pending.add(4,-1,0,0,0);pending.cover(-1,0,100);
  require(pending.edits().size()==1,"no revision or frame timeout ghosts");
  pending.resolve(4,true,100);pending.cover(-1,0,base.authoritative_revision=100);
  require(pending.edits().empty(),"baseline before receipt retires on resolve cover");
  for(std::uint64_t id=10;id<10+PredictedBlocks::Capacity;++id)
    require(pending.add(id,-1,0,0,0),"bounded capacity fill");
  require(!pending.add(999,-1,0,0,0),"bounded backpressure");
  pending.clear();
  require(pending.can_submit() && base.blocks[31]==7,"reconnect clears predictions not durable base");
  std::cout<<"block_prediction_qualification PASS rejection_same_revision rapid_edits unrelated_revision covering_ack bounded_queue reconnect mesh_source_restore\n";
  } catch(const std::exception& error) {
    std::cerr<<"block_prediction_qualification FAIL "<<error.what()<<std::endl;
    return 1;
  }
}
