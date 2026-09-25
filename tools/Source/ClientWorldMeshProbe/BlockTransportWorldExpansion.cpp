#include "BlockTransportWorldAdmission.h"

namespace mesh_probe::world_admission {
namespace {
struct Result {BlockSurfaceKey key;Pixel reflectance;Words state;};
static_assert(sizeof(Result)==48);
struct Request {Words value;BlockSurfaceKey expected;unsigned count;bool valid;float domain;};
unsigned count(Fixture& f,const Face& face) {
  const auto& block=f.catalog[face[3]&65535];const unsigned direction=(face[3]>>16)&15;
  if(!block.opaque || block.fluidKind!="none" || block.requiresSolidBase)return 0;
  if(block.sprite)return direction==6 || direction==8?2:0;
  return direction<6?(((face[3]>>20)&31)+1)*(((face[3]>>25)&31)+1):0;
}
void run(Fixture& f,const char* name,const std::vector<Face>& faces,rhi::IBuffer* input) {
  auto& r=f.renderer;std::vector<Request> tests;std::vector<Words> requests;
  for(unsigned index=0;index<faces.size();++index) {
    const auto& face=faces[index];const unsigned total=count(f,face),direction=(face[3]>>16)&15;
    const auto& block=f.catalog[face[3]&65535];const auto layer=world_atlas_preview_layer(r.atlas,face[3]&65535);
    for(unsigned ordinal=0;ordinal<=total;++ordinal) {
      BlockSurfaceKey result{0,0,0,UINT32_MAX};bool valid=ordinal<total;
      if(valid) {
        std::int64_t x=std::bit_cast<int>(face[0]),y=std::bit_cast<int>(face[1]),z=std::bit_cast<int>(face[2]);
        unsigned tag_value=direction;
        if(block.sprite)tag_value=(layer<<4)|(direction+ordinal);
        else {
          const unsigned width=((face[3]>>20)&31)+1,u=ordinal%width,v=ordinal/width;
          if(direction<2) {y+=v;z+=u;}
          else if(direction<4) {x+=u;z+=v;}
          else {x+=u;y+=v;}
          valid=x<=INT32_MAX && y<=INT32_MAX && z<=INT32_MAX;
        }
        if(valid)result={int(x),int(y),int(z),tag_value};
      }
      tests.push_back({{index,ordinal,0,0},result,total,valid,block.sprite?0.f:block.occlusion?1.f:-float(layer+1)});
      requests.push_back(tests.back().value);
    }
  }
  const auto start=std::chrono::steady_clock::now();r.active_frame=r.frame_queue.slot(r.frames);
  require(r.frame_queue.wait(r.active_frame,2000),"BT world expansion frame reuse");
  Slang::ComPtr<rhi::IComputePipeline> pipeline;
  const auto path=(std::filesystem::path(__FILE__).parent_path()/"BlockTransportWorldAdmission.slang").generic_string();
  require(block_transport_pipeline(r.device,path.c_str(),"main",pipeline),"BT production world expansion helper pipeline");
  const auto request_buffer=buffer(r,requests.data(),requests.size()*sizeof(Words),sizeof(Words),rhi::BufferUsage::ShaderResource);
  std::vector<Result> actual(requests.size());const auto rw=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess;
  const auto output=buffer(r,actual.data(),actual.size()*sizeof(Result),sizeof(Result),rw);
  auto commands=r.queue->createCommandEncoder();require(bool(commands),"BT world expansion encoder");
  auto* pass=commands->beginComputePass();require(pass!=nullptr,"BT world expansion pass");
  auto* root=pass->bindPipeline(pipeline);require(root!=nullptr,"BT world expansion pipeline bind");
  require(bind_world_atlas(r.atlas,root),"BT world expansion atlas bindings");const rhi::ShaderCursor cursor(root);
  checked(cursor["worldExpansionFaces"].setBinding(input),"BT world expansion actual packed faces");
  checked(cursor["worldExpansionRequests"].setBinding(request_buffer),"BT world expansion bounded requests");
  checked(cursor["worldExpansionResults"].setBinding(output),"BT world expansion output");
  const unsigned total=unsigned(requests.size());checked(cursor["worldExpansionCount"].setData(&total,sizeof(total)),"BT world expansion request count");
  pass->dispatchCompute((total+63)/64,1,1);pass->end();submit(r,commands);
  checked(r.device->readBuffer(output,0,actual.size()*sizeof(Result),actual.data()),"BT world expansion result readback");
  for(std::size_t i=0;i<actual.size();++i) {
    const auto& a=actual[i];const auto& e=tests[i];
    require(a.state[0]==e.count && (a.state[1]!=0)==e.valid && a.key==e.expected,
        "BT world face expansion differs from independent signed integer oracle");
    if(e.valid)require(a.reflectance[3]==e.domain,"BT cutout/plant reconstruction classification changed");
    for(unsigned c=0;c<3;++c)require(std::isfinite(a.reflectance[c]) && a.reflectance[c]>=0 && a.reflectance[c]<=1,
        "BT world expansion invalid reflectance");
  }
  cap(r,start);
  std::printf("block_transport_world_expansion=passed case=%s packed_faces=%zu requests=%zu exact_integer_keys=1 signed_overflow=1 material_flags=1\n",name,faces.size(),tests.size());
}
}
void expansion(Fixture& f,const Mesh& mesh) {
  run(f,"resident_mesh",mesh.faces,mesh.gpu.faces);
  std::vector<Face> limits;const unsigned stone=material(f,"stone");
  for(unsigned direction=0;direction<6;++direction) {
    const unsigned packed=stone|(direction<<16)|(31u<<20)|(31u<<25);
    limits.push_back({std::bit_cast<unsigned>(INT32_MIN),std::bit_cast<unsigned>(INT32_MIN),std::bit_cast<unsigned>(INT32_MIN),packed});
    limits.push_back({std::bit_cast<unsigned>(INT32_MAX-15),std::bit_cast<unsigned>(INT32_MAX-15),std::bit_cast<unsigned>(INT32_MAX-15),packed});
  }
  const auto input=buffer(f.renderer,limits.data(),limits.size()*sizeof(Face),sizeof(Face),rhi::BufferUsage::ShaderResource);
  run(f,"integer_limits",limits,input);
}
}
