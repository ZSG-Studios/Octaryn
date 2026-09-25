#include "BlockTransportWork.h"
#include <algorithm>
#include <bit>
#include <set>

namespace mesh_probe {
namespace {
using Pixel=std::array<float,4>;
using Words=std::array<unsigned,4>;
constexpr unsigned Geometry=307,Radiance=311;
struct Statistics {unsigned frames{},guards{},rows{},fields{};};
class SelectionProbe {
  WorldRenderer& r;
  BlockTransportWork work;
  Slang::ComPtr<rhi::IBuffer> surface_buffer,direct_buffer,environment_buffer,indirect_buffer,accepted;
  Slang::ComPtr<rhi::IComputePipeline> guard_pipeline;
  std::array<unsigned,16> previous{};
  Statistics& stats;
  bool dirty=true,reset_header=false;
  void submit(rhi::ICommandEncoder* commands) {
    auto command=commands->finish();require(bool(command),"BT scheduler command finish");
    require(r.frame_queue.submit(r.queue,command,r.active_frame) && r.frame_queue.wait(r.active_frame,2000),
        "BT scheduler bounded completion");
  }
  void upload(rhi::ICommandEncoder* commands,rhi::IBuffer* target,const void* bytes,std::size_t size) {
    checked(commands->uploadBufferData(target,0,size,bytes),"BT scheduler fixture upload");
    commands->setBufferState(target,rhi::ResourceState::ShaderResource);
  }
  bool ready(unsigned row)const {
    const auto& s=surfaces[row];
    return s.state[2]!=UINT32_MAX && s.state[3]>0 && std::bit_cast<unsigned>(direct[row][3])==radiance &&
        std::bit_cast<unsigned>(environment[row][3])==radiance && std::bit_cast<unsigned>(indirect[row][3])==radiance;
  }
public:
  unsigned geometry=Geometry,radiance=Radiance,frame=401;
  std::vector<BlockTransportSurface> surfaces;
  std::vector<Pixel> direct,environment,indirect;
  std::vector<Words> selected;
  SelectionProbe(WorldRenderer& renderer,unsigned capacity,unsigned budget,Statistics& output):
      r(renderer),work(r,capacity,budget),stats(output),surfaces(capacity),direct(capacity),environment(capacity),
      indirect(capacity),selected(budget) {
    const auto usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::CopyDestination;
    surface_buffer=buffer(r,surfaces.data(),surfaces.size()*sizeof(surfaces[0]),sizeof(surfaces[0]),usage);
    direct_buffer=buffer(r,direct.data(),direct.size()*sizeof(Pixel),sizeof(Pixel),usage);
    environment_buffer=buffer(r,environment.data(),environment.size()*sizeof(Pixel),sizeof(Pixel),usage);
    indirect_buffer=buffer(r,indirect.data(),indirect.size()*sizeof(Pixel),sizeof(Pixel),usage);
    std::vector<unsigned> zero(budget);
    accepted=buffer(r,zero.data(),zero.size()*sizeof(unsigned),sizeof(unsigned),rhi::BufferUsage::UnorderedAccess);
    const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportSelect.slang").generic_string();
    require(block_transport_pipeline(r.device,path.c_str(),"main",guard_pipeline),"BT scheduler production ticket guard");
  }
  void active(unsigned row,unsigned generation=7) {
    auto& surface=surfaces[row];surface.key={int(row),-17,43,3};surface.state={geometry,0,frame,1};
    surface.extra={generation,0,0,0};
    direct[row]=environment[row]=indirect[row]=Pixel{1,2,3,std::bit_cast<float>(radiance)};dirty=true;
  }
  void start(unsigned row) {previous.fill(0);previous[0]=row;reset_header=true;}
  void changed() {dirty=true;}
  void run() {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT scheduler frame reuse");
    const unsigned capacity=work.capacity,scan_start=previous[0];
    std::vector<Words> expected,blocks((capacity+255)/256);
    unsigned occupied=0,current=0,resident=0,last_offset=0;
    for(unsigned offset=0;offset<capacity;++offset) {
      const unsigned row=(scan_start+offset)%capacity;
      if(surfaces[row].state[0]!=geometry)continue;
      ++occupied;++blocks[offset/256][0];
      if((surfaces[row].extra[3]&1)!=0){++resident;++blocks[offset/256][3];}
      if(ready(row)){++current;++blocks[offset/256][2];}
      if(expected.size()<work.maximum) {
        expected.push_back({row,surfaces[row].extra[0],geometry,surfaces[row].extra[3]>>2});last_offset=offset;
      }
    }
    unsigned prefix=0;for(auto& block:blocks){block[1]=prefix;prefix+=block[0];}
    auto expected_header=previous;
    expected_header[1]=scan_start;expected_header[2]=unsigned(expected.size());expected_header[3]=occupied;
    expected_header[4]=current;expected_header[5]=geometry;expected_header[6]=radiance;expected_header[7]=frame;
    expected_header[11]=resident;
    if(!expected.empty()) {
      expected_header[0]=(expected.back()[0]+1)%capacity;
      if(scan_start+last_offset+1>=capacity)++expected_header[8];
    }
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT scheduler encoder");
    if(dirty) {
      upload(commands,surface_buffer,surfaces.data(),surfaces.size()*sizeof(surfaces[0]));
      upload(commands,direct_buffer,direct.data(),direct.size()*sizeof(Pixel));
      upload(commands,environment_buffer,environment.data(),environment.size()*sizeof(Pixel));
      upload(commands,indirect_buffer,indirect.data(),indirect.size()*sizeof(Pixel));dirty=false;
    }
    if(reset_header){upload(commands,work.schedule,previous.data(),sizeof(previous));reset_header=false;}
    const Words info{geometry,frame,capacity,16};
    work.encode(commands,surface_buffer,direct_buffer,environment_buffer,indirect_buffer,info,radiance);submit(commands);
    const auto header=work.header();require(header==expected_header,"BT scheduler header differs from independent cyclic scan");
    checked(r.device->readBuffer(work.rows,0,selected.size()*sizeof(Words),selected.data()),"BT scheduler row readback");
    std::vector<Words> actual_blocks(blocks.size());
    checked(r.device->readBuffer(work.blocks,0,blocks.size()*sizeof(Words),actual_blocks.data()),"BT scheduler prefix readback");
    require(actual_blocks==blocks,"BT scheduler block totals or prefixes differ from CPU occupancy");
    std::set<unsigned> unique;
    for(unsigned i=0;i<expected.size();++i) {
      require(selected[i]==expected[i] && unique.insert(selected[i][0]).second,"BT scheduler omitted, duplicated or reordered a live row");
      ++stats.rows;stats.fields+=4;
    }
    stats.fields+=16+unsigned(blocks.size())*4;previous=header;++frame;++stats.frames;
    block_transport_complete(r,start);
  }
  void guard(unsigned fault) {
    const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
    require(r.frame_queue.wait(r.active_frame,2000),"BT work guard frame reuse");
    auto header=previous;auto tickets=selected;auto records=surfaces;
    require(header[2]>1,"BT work guard requires two selected rows");
    if(fault==1)++records[tickets[0][0]].extra[0];
    if(fault==2)--header[5];if(fault==3)--header[6];if(fault==4)--header[7];
    if(fault==5)tickets[0][0]=work.capacity;if(fault==6)--tickets[0][2];
    if(fault==7)records[tickets[0][0]].state[0]=UINT32_MAX-1;
    if(fault==8)header[2]=0;
    auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT work guard encoder");
    upload(commands,surface_buffer,records.data(),records.size()*sizeof(records[0]));
    upload(commands,work.rows,tickets.data(),tickets.size()*sizeof(Words));
    upload(commands,work.schedule,header.data(),sizeof(header));
    auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT work guard pass");
    auto* root=pass->bindPipeline(guard_pipeline);require(root!=nullptr,"BT work guard pipeline");
    work.bind(root);const rhi::ShaderCursor cursor(root);
    checked(cursor["btSurfaces"].setBinding(surface_buffer),"BT work guard surfaces");
    checked(cursor["probeAccepted"].setBinding(accepted),"BT work guard results");
    const Words info{geometry,previous[7],work.capacity,16},budget{0,work.maximum,radiance,0};
    checked(cursor["btFrameInfo"].setData(info.data(),sizeof(info)),"BT work guard frame");
    checked(cursor["btWork"].setData(budget.data(),sizeof(budget)),"BT work guard budget");
    pass->dispatchCompute((work.maximum+63)/64,1,1);pass->end();submit(commands);
    std::vector<unsigned> actual(work.maximum);
    checked(r.device->readBuffer(accepted,0,actual.size()*sizeof(unsigned),actual.data()),"BT work guard readback");
    for(unsigned i=0;i<actual.size();++i) {
      const bool stale_all=(fault>=2 && fault<=4)||fault==8;
      const bool stale_first=(fault==1 || fault==5 || fault==6 || fault==7) && i==0;
      const unsigned expected=stale_all || stale_first || i>=previous[2]?UINT32_MAX:selected[i][0];
      require(actual[i]==expected,"BT production work guard accepted a stale epoch or recycled slot ticket");++stats.fields;
    }
    ++stats.guards;block_transport_complete(r,start);
  }
};
}
void block_transport_select_cases(Fixture& fixture) {
  Statistics stats;
  {
    // A partial final group also proves that padding lanes never enter the queue.
    SelectionProbe probe(fixture.renderer,513,7,stats);probe.start(510);probe.run();
    for(unsigned row:{0u,1u,4u,9u,255u,256u,300u,509u,510u,512u})probe.active(row,row+3);
    probe.surfaces[0].extra[3]=(9u<<2)|3u;probe.surfaces[512].extra[3]=(17u<<2)|1u;
    probe.surfaces[2].state[0]=UINT32_MAX;probe.surfaces[3].state[0]=UINT32_MAX-1;
    probe.surfaces[5].state[0]=Geometry-1;
    probe.direct[0][3]=std::bit_cast<float>(Radiance-1);probe.environment[1][3]=std::bit_cast<float>(Radiance-1);
    probe.indirect[4][3]=std::bit_cast<float>(Radiance-1);probe.surfaces[9].state[2]=UINT32_MAX;
    probe.surfaces[255].state[3]=0;probe.changed();probe.run();probe.run();probe.run();
    for(unsigned fault=0;fault<=8;++fault)probe.guard(fault);
    ++probe.geometry;++probe.radiance;probe.start(400);probe.active(3,17);probe.active(512,29);probe.run();
    probe.surfaces[3].state[0]=UINT32_MAX-1;probe.active(4,UINT32_MAX);probe.changed();probe.run();
  }
  {
    SelectionProbe probe(fixture.renderer,BlockTransportCapacity,BlockTransportRows,stats);
    std::vector<unsigned> visits(BlockTransportCapacity);
    for(unsigned row=0;row<BlockTransportCapacity;++row)probe.active(row,1+row%31);
    probe.start(BlockTransportCapacity-5);
    for(unsigned batch=0;batch<BlockTransportCapacity/BlockTransportRows;++batch) {
      probe.run();for(const auto& selected:probe.selected)++visits[selected[0]];
    }
    require(std::all_of(visits.begin(),visits.end(),[](unsigned count){return count==1;}),
        "BT full production cache starvation or duplicate selection within one cyclic sweep");
  }
  require(fixture.renderer.debug.errors.load()==0,"BT occupied-row scheduler graphics validation errors");
  require(stats.frames==38 && stats.guards==9 && stats.rows>=BlockTransportCapacity,"BT scheduler fixture scope changed");
  std::printf("block_transport_select=passed hardware=1 production_select=1 holes=1 tombstones=1 partial_groups=1 cyclic_fairness=1 ready_epochs=1 work_guard=1 slot_generation=1 geometry_epochs=1 no_screen_resources=1 capacity=%u maximum=%u full_sweep_rows=%u selection_frames=%u guard_cases=%u selected_rows=%u scalar_checks=%u validation_errors=0\n",
      BlockTransportCapacity,BlockTransportRows,BlockTransportCapacity,stats.frames,stats.guards,stats.rows,stats.fields);
}
}
