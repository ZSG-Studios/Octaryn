#include "BlockTransportWorldAdmission.h"
#include "../../../octaryn-client/Source/Rendering/BlockTransportGI/BlockTransportAdmission.h"

namespace mesh_probe::world_admission {
namespace {
void scheduling() {
  Key low{},high{};
  require(block_admission_bounds({-.1f,-32.01f,16777216,0},low,high) &&
      low==Key{-33,-65,16777184,0} && high==Key{31,-1,16777248,0},
      "BT world scheduling floors negative and large positions incorrectly");
  require(!block_admission_bounds({NAN,0,0,0},low,high) &&
      !block_admission_bounds({float(INT32_MIN),0,0,0},low,high) &&
      !block_admission_bounds({float(INT32_MAX),0,0,0},low,high),"BT admission unsafe integer region accepted");
  for(const auto value:{INT32_MIN,-65,-64,-33,-32,-1,0,31,32,INT32_MAX})
    require(block_admission_column(value)==std::int32_t(std::floor(double(value)/32)),
        "BT world column floor disagrees at signed boundary");
  for(int anchor=-64;anchor<64;++anchor) {
    require(block_admission_bounds({float(anchor),0,float(anchor),0},low,high),"BT finite admission region rejected");
    const auto columns=(block_admission_column(high[0]-1)-block_admission_column(low[0])+1)*
        (block_admission_column(high[2]-1)-block_admission_column(low[2])+1);
    require(columns>=4 && columns<=int(BlockAdmissionColumns),"BT 64-cell region exceeds nine-column allocation");
  }
  BlockAdmissionColumn boundary{0,0,33,31,0};const auto last=block_admission_advance(boundary,16);
  require(last.offset==31 && last.count==2 && boundary.offset==0 && boundary.sweeps==1,
      "BT world admission range wrapped inside one GPU dispatch");
  const auto next=block_admission_advance(boundary,16);
  require(next.offset==0 && next.count==16 && boundary.offset==16 && boundary.sweeps==1,
      "BT world admission failed next-sweep progress");
  const auto zero=block_admission_advance(boundary,0);
  require(zero.count==0 && boundary.offset==16,"BT zero admission budget changed progress");
  const unsigned sizes[9]={1,7,64,257,4097,2,33,1024,3};
  std::array<BlockAdmissionColumn,9> columns{};std::array<std::vector<unsigned>,9> visits;
  for(unsigned i=0;i<9;++i) {columns[i].faces=sizes[i];visits[i].resize(sizes[i]);}
  bool complete=false;
  for(unsigned frame=0;frame<200 && !complete;++frame) {
    unsigned work=0;complete=true;
    for(unsigned i=0;i<9;++i) {
      if(columns[i].sweeps)continue;complete=false;
      const unsigned quota=BlockAdmissionFaces/9+(i<BlockAdmissionFaces%9?1u:0u);
      const auto range=block_admission_advance(columns[i],quota);work+=range.count;
      require(range.count<=quota && range.offset+range.count<=sizes[i],"BT admission bounded stream range");
      for(unsigned face=range.offset;face<range.offset+range.count;++face)++visits[i][face];
    }
    require(work<=BlockAdmissionFaces,"BT nine-column admission exceeded 256-face frame budget");
  }
  require(complete,"BT mixed tiny/large world streams did not complete within bounded sweeps");
  for(const auto& stream:visits)for(unsigned count:stream)
    require(count==1,"BT first resident-world sweep omitted or duplicated a packed face");
}
}
void budget(Fixture& f,Cache& cache) {
  scheduling();auto& r=f.renderer;const auto before=cache.snapshot();
  std::array<unsigned,12> pressure{};pressure[8]=unsigned(before.size());pressure[11]=1;
  std::vector<Pixel> values(Capacity,Pixel{1,2,3,4});
  const auto rw=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
  const auto counters=buffer(r,pressure.data(),sizeof(pressure),sizeof(unsigned),rw);
  const auto direct=buffer(r,values.data(),values.size()*sizeof(Pixel),sizeof(Pixel),rw);
  const auto environment=buffer(r,values.data(),values.size()*sizeof(Pixel),sizeof(Pixel),rw);
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  require(block_transport_pipeline(r.device,"octaryn-client/Shaders/BlockTransportGI/Evict.slang","main",pipeline),
      "BT resident pin production eviction pipeline");
  const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT resident pin frame reuse");
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT resident pin encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT resident pin pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT resident pin pipeline bind");
  const rhi::ShaderCursor cursor(root);const Words frame{Epoch,500,Capacity,Links};
  checked(cursor["btFrameInfo"].setData(frame.data(),sizeof(frame)),"BT resident pin aged frame");
  checked(cursor["btSurfaces"].setBinding(cache.surfaces),"BT resident pin surfaces");
  checked(cursor["btCounters"].setBinding(counters),"BT resident pin actual pressure");
  checked(cursor["btDirect"].setBinding(direct),"BT resident pin direct history");
  checked(cursor["btEnvironment"].setBinding(environment),"BT resident pin environment history");
  pass->dispatchCompute((Capacity+63)/64,1,1);pass->end();submit(r,commands);
  require(cache.snapshot()==before,"BT current resident world rows were evicted under pressure");
  checked(r.device->readBuffer(counters,0,sizeof(pressure),pressure.data()),"BT resident pin live counters");
  require(pressure[8]==before.size() && pressure[9]==0,"BT resident pin eviction changed live gauges");
  cap(r,start);
  std::printf("block_transport_world_budget=passed production_scheduler=1 signed_bounds=1 max_columns=9 max_faces=256 no_range_wrap=1 completed_sweeps=1 production_eviction=1 resident_pins=1\n");
}
}
