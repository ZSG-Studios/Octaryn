#include "Probe.h"
#include "WorldFrames.h"
#include "WorldGpuProfile.h"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace mesh_probe {
namespace {
constexpr unsigned Words=16,FrameCount=257;
std::uint32_t sentinel(unsigned frame,unsigned word) {return 0x51a70000u+frame*257+word;}
struct Copies {
  WorldRenderer& r;
  std::array<Slang::ComPtr<rhi::IBuffer>,2> upload,readback;
  std::array<void*,2> mapped{};
  explicit Copies(WorldRenderer& renderer):r(renderer) {
    for(unsigned slot=0;slot<2;++slot) {
      rhi::BufferDesc desc{};desc.size=Words*sizeof(std::uint32_t);
      desc.usage=rhi::BufferUsage::CopySource;desc.defaultState=rhi::ResourceState::CopySource;
      desc.memoryType=rhi::MemoryType::Upload;
      checked(r.device->createBuffer(desc,nullptr,upload[slot].writeRef()),"frame upload creation");
      checked(r.device->mapBuffer(upload[slot],rhi::CpuAccessMode::Write,&mapped[slot]),"frame upload mapping");
      require(mapped[slot]!=nullptr,"frame upload mapping is null");
      desc.usage=rhi::BufferUsage::CopyDestination;desc.defaultState=rhi::ResourceState::CopyDestination;
      desc.memoryType=rhi::MemoryType::ReadBack;
      checked(r.device->createBuffer(desc,nullptr,readback[slot].writeRef()),"frame readback creation");
    }
  }
  ~Copies() {
    // Also protects diagnostic exception paths after a successful submission.
    r.queue->waitOnHost();
    for(unsigned slot=0;slot<2;++slot)if(mapped[slot])r.device->unmapBuffer(upload[slot]);
  }
  void write(unsigned slot,unsigned frame) {
    std::array<std::uint32_t,Words> values{};
    for(unsigned i=0;i<Words;++i)values[i]=sentinel(frame,i);
    std::memcpy(mapped[slot],values.data(),sizeof(values));
  }
  void verify(unsigned slot,unsigned frame) {
    void* data{};checked(r.device->mapBuffer(readback[slot],rhi::CpuAccessMode::Read,&data),"frame readback mapping");
    require(data!=nullptr,"frame readback mapping is null");
    std::array<std::uint32_t,Words> actual{};std::memcpy(actual.data(),data,sizeof(actual));
    r.device->unmapBuffer(readback[slot]);
    for(unsigned i=0;i<Words;++i)require(actual[i]==sentinel(frame,i),"frame copy sentinel belongs to another slot or frame");
  }
};
std::vector<std::string> fields(const std::string& line) {
  std::vector<std::string> result;std::istringstream stream(line);std::string field;
  while(std::getline(stream,field,','))result.push_back(field);
  return result;
}
void verify_csv(const std::filesystem::path& path) {
  std::ifstream file(path);std::string line;require(bool(std::getline(file,line)),"frame profile header missing");
  const auto header=fields(line);std::map<std::string,std::size_t> indices;
  for(std::size_t i=0;i<header.size();++i)indices.emplace(header[i],i);
  unsigned frame=0;
  while(std::getline(file,line)) {
    const auto values=fields(line);require(values.size()==header.size(),"frame profile row shape");
    const auto number=[&](const char* name){return std::stod(values.at(indices.at(name)));};
    require(frame<FrameCount && number("frame")==101+frame,"frame profile reordered, duplicated or lost a frame");
    require(number("columns")==201+frame && number("quads")==301+frame &&
        number("drawn_columns")==401+frame && number("drawn_quads")==501+frame &&
        number("width")==601+frame && number("height")==701+frame &&
        number("world_batch")==frame%2 && number("world_draw_commands")==801+frame &&
        number("world_draw_columns")==901+frame && number("frames_in_flight")==2,
        "frame profile metadata was overwritten by another slot");
    require(number("mesh_jobs_started")==1001+frame && number("mesh_count_submits")==1101+frame &&
        number("mesh_emit_submits")==1201+frame && number("halo_published")==1301+frame &&
        number("halo_discarded")==1401+frame && number("mesh_decode_ms")==frame+.25 &&
        number("wait_cpu_ms")==frame+.125,"frame profile counters or CPU markers lost association");
    for(const auto* name:{"sky_ms","opaque_ms","hdr_ms","forward_ms","fsr_ms","tonemap_ms","ui_ms","copy_ms","total_gpu_ms",
        "mesh_cpu_ms","atlas_cpu_ms","acquire_cpu_ms","prepare_cpu_ms","encode_cpu_ms","submit_cpu_ms","present_cpu_ms"})
      require(std::isfinite(number(name)) && number(name)>=0,"frame profile timing is invalid");
    ++frame;
  }
  require(frame==FrameCount,"odd final drain omitted a frame profile row");
}
}
void frames_cases(Fixture& fixture) {
  auto& r=fixture.renderer;WorldFrames frames;
  require(frames.initialize(r.device,2) && frames.count()==2,"two frame slots initialization");
  require(!frames.initialize(r.device,2) && !frames.wait(2),"frame slot invalid lifecycle accepted");
  Copies copies(r);
  require(copies.upload[0].get()!=copies.upload[1].get() && copies.readback[0].get()!=copies.readback[1].get() &&
      copies.mapped[0]!=copies.mapped[1],"frame copies share mutable resources");
  const auto path=std::filesystem::path("logs/client/world-frames-probe.csv");
  {
    WorldGpuProfile profile(r.device,path.string().c_str());
    std::array<int,2> previous{-1,-1};
    for(unsigned frame=0;frame<FrameCount;++frame) {
      const auto slot=frames.slot(frame);require(slot==frame%2,"frame ring did not alternate");
      require(frames.wait(slot),"frame slot completion wait");
      if(previous[slot]>=0)copies.verify(slot,static_cast<unsigned>(previous[slot]));
      require(profile.resolve(slot),"completed timestamp slot did not resolve");
      profile.begin_cpu(slot,frame+.125);copies.write(slot,frame);
      auto commands=r.queue->createCommandEncoder();require(commands!=nullptr,"frame copy encoder");
      require(profile.begin(commands),"frame timestamp begin");
      commands->copyBuffer(copies.readback[slot],0,copies.upload[slot],0,Words*sizeof(std::uint32_t));
      for(unsigned i=0;i<7;++i){profile.mark(commands);profile.mark_cpu();}
      profile.mark(commands);
      auto command=commands->finish();require(command!=nullptr,"frame copy finish");
      require(frames.submit(r.queue,command,slot),"frame copy submission");
      require(!frames.submit(r.queue,command,slot),"pending frame accepted a second submission");
      WorldMeshTimings mesh{};mesh.halo_decode=frame+.25;
      mesh.jobs_started=1001+frame;mesh.count_submits=1101+frame;mesh.emit_submits=1201+frame;
      mesh.halo_published=1301+frame;mesh.halo_discarded=1401+frame;
      require(profile.finish(101+frame,201+frame,301+frame,401+frame,501+frame,
          601+int(frame),701+int(frame),frame%2!=0,801+frame,901+frame,mesh,2),"frame metadata finish");
      bool rejected=false;
      try {profile.begin_cpu(slot,0);}catch(const std::runtime_error&){rejected=true;}
      require(rejected,"pending profile slot accepted overwrite");
      previous[slot]=static_cast<int>(frame);
      if(frame==1) {
        std::uint32_t pending{};std::memcpy(&pending,copies.mapped[0],sizeof(pending));
        require(pending==sentinel(0,0),"second slot overwrote first pending upload");
      }
    }
    require(frames.drain(),"odd final frame fence drain");
    for(unsigned slot=0;slot<2;++slot)copies.verify(slot,static_cast<unsigned>(previous[slot]));
    require(profile.drain() && profile.drain(),"odd final timestamp drain");
  }
  verify_csv(path);
  require(r.debug.errors.load()==0,"frame ownership validation errors");
  std::printf("world_frames=passed slots=2 frames=%u double_submit=rejected sentinels=exact profile_association=exact odd_drain=ordered\n",FrameCount);
}
}
