#include "SceneGeometryPool.h"
#include "SceneMemoryLedger.h"
#include "SceneRootPages.h"
#include "GeometryCook.h"
#include "../MapWorld/MapTextureCache.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
namespace {
void check(bool value,const std::string& text) {if(!value)throw std::runtime_error(text);}
struct Fixture {std::filesystem::path path;GeometryAsset asset;std::vector<std::uint8_t> decoded;};
Fixture fixture(const std::filesystem::path& directory,unsigned id,bool compact) {
  MapModel model;model.vertices.resize(3);model.indices={0,1,2};model.primitives.resize(1);model.primitives[0].index_count=3;
  model.vertices[1].position[0]=1;model.vertices[2].position[1]=1;
  for(auto& vertex:model.vertices) {
    vertex.position[2]=float(id);vertex.normal[2]=1;
    if(!compact)vertex.tangent[0]=vertex.tangent[3]=1;
  }
  const auto* bytes=reinterpret_cast<const std::uint8_t*>(&id);const auto hash=map_texture_digest({bytes,sizeof(id)});
  Fixture result;result.path=directory/(std::to_string(id)+".vgeom");std::string error;GeometryAsset cooked;
  check(cook_geometry(model,hash,cooked,error,{compact}),error);
  check(cooked.pages.size()==1,"shared pool fixture is not one coarse page");
  check(decode_geometry_page(cooked,0,result.decoded,error),error);
  check(write_geometry_cache(result.path,cooked,error),error);
  check(read_geometry_cache(result.path,hash,result.asset,error,false),error);return result;
}
}
bool probe_scene_pool(rhi::IDevice* device,rhi::ICommandQueue* queue) {
  try {
    const auto token=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory=std::filesystem::path("logs/tools/scene-page-pool")/std::to_string(token);
    std::filesystem::create_directories(directory);
    std::array<Fixture,4> assets{fixture(directory,1,true),fixture(directory,2,false),fixture(directory,3,true),fixture(directory,4,true)};
    auto ledger=std::make_shared<SceneMemoryLedger>(3*page_bytes);
    auto pool=std::make_shared<SceneGeometryPool>();SceneGeometryPoolConfig config;
    config.slots=3;config.root_slots=1;config.maximum_pages=16;config.maximum_assets=8;
    config.workers=2;config.feedback_capacity=8;config.upload_pages=2;config.upload_ms=5;
    check(pool->initialize(device,ledger,config),pool->error());
    std::array<SceneGeometryHandle,4> handles{};SceneGeometryHandle duplicate;
    for(unsigned i=0;i<2;++i)check(pool->register_asset(assets[i].path,assets[i].asset,handles[i]),pool->error());
    check(pool->register_asset(assets[0].path,assets[0].asset,duplicate) && duplicate==handles[0],"identical asset did not share page registration");
    pool->release(duplicate);
    check(ledger->stats().used==3*page_bytes && !ledger->reserve(1,SceneMemoryDomain::Pages),"shared pool physical budget changed with assets");
    auto fence=device->createFence({});check(bool(fence),"pool fence");std::uint64_t signal{};
    const auto start=std::chrono::steady_clock::now();
    const auto submit=[&](rhi::ICommandEncoder* encoder) {
      auto commands=encoder->finish();check(bool(commands),"pool command finish");auto* command=commands.get();auto* f=fence.get();++signal;
      rhi::SubmitDesc desc{};desc.commandBuffers=&command;desc.commandBufferCount=1;
      desc.signalFences=&f;desc.signalFenceValues=&signal;desc.signalFenceCount=1;
      check(SLANG_SUCCEEDED(queue->submit(desc)),"pool submit");
      check(pool->submitted(fence,signal) && pool->submitted(fence,signal),pool->error());
      check(SLANG_SUCCEEDED(device->waitForFences(1,&f,&signal,true,30'000'000'000ull)),"pool fence deadline");
    };
    while(!pool->roots_ready(handles[0]) || !pool->roots_ready(handles[1])) {
      check(std::chrono::steady_clock::now()-start<std::chrono::seconds(30),"pool decode deadline");
      auto commands=queue->createCommandEncoder();
      for(unsigned i=0;i<2;++i)check(pool->record(commands,handles[i],{}),pool->error());
      submit(commands);std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto first=pool->page_table(handles[0])[0],second=pool->page_table(handles[1])[0];
    const auto first_size=geometry_page_payload_bytes(assets[0].asset)[0];
    const auto second_size=geometry_page_payload_bytes(assets[1].asset)[0];
    check(first.resident && second.resident && first.slot==second.slot && first.reserved+first_size<=second.reserved,
        "independent roots failed packed slab placement");
    for(unsigned i=0;i<2;++i) {
      const auto page=pool->page_table(handles[i])[0];const auto size=geometry_page_payload_bytes(assets[i].asset)[0];
      std::vector<std::uint8_t> gpu(size);
      check(SLANG_SUCCEEDED(device->readBuffer(pool->buffer(),std::uint64_t(page.slot)*page_bytes+page.reserved,size,gpu.data())),"packed pool GPU read");
      check(std::equal(gpu.begin(),gpu.end(),assets[i].decoded.begin()),"packed payload changed during shared upload");
    }
    auto commands=queue->createCommandEncoder();
    check(pool->record(commands,handles[0],{}) && pool->record(commands,handles[1],{}),pool->error());
    pool->release(handles[0]);
    check(pool->register_asset(assets[2].path,assets[2].asset,handles[2]),pool->error());
    const auto pending=pool->page_table(handles[2])[0];
    check(pending.reserved!=first.reserved,"recorded consumer root slice reused before submission");
    submit(commands);commands.setNull();
    // Polling a completed timeline retires the recorded consumer before the next registration.
    check(pool->register_asset(assets[3].path,assets[3].asset,handles[3]),pool->error());
    const auto replacement=pool->page_table(handles[3])[0];
    check(replacement.slot==first.slot && replacement.reserved==first.reserved && replacement.generation!=first.generation,
        "completed packed root did not reuse a new generation");
    check(pool->page_table(handles[0]).empty(),"retired asset handle still resolves");
    const auto stats=pool->stats();
    check(stats.uploaded_bytes==first_size+second_size && stats.uploaded_bytes<2*page_bytes,"packed roots uploaded full padded pages");
    for(unsigned i=1;i<4;++i)pool->release(handles[i]);pool.reset();
    check(ledger->stats().used==0,"shared page allocation lease leaked");
    std::printf("scene_pool_gpu passed=1 packed_assets=2 compact_authored=1 local_page_zero=isolated deduplicated=1 retirement_generation=1 uploaded_bytes=%llu physical_bytes=%u\n",
        static_cast<unsigned long long>(stats.uploaded_bytes),3*page_bytes);return true;
  }catch(const std::exception& error) {std::fprintf(stderr,"scene_pool_gpu failed=%s\n",error.what());return false;}
}
