#include "ShaderCache.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>
using namespace octaryn::client::rendering;
void shader_cache_eviction_cases(const std::filesystem::path&);
int shader_cache_process_case(const std::filesystem::path&,const char*);
namespace {
unsigned checks{};
void require(bool ok,const char* label) {
  ++checks;if(!ok) {std::fprintf(stderr,"shader_cache_test failed: %s\n",label);std::exit(1);}
}
class View final:public ISlangBlob {
  const void* data;std::size_t size;
public:
  View(const void* value,std::size_t length):data(value),size(length) {}
  SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID&,void**) override {return SLANG_E_NO_INTERFACE;}
  SLANG_NO_THROW std::uint32_t SLANG_MCALL addRef() override {return 1;}
  SLANG_NO_THROW std::uint32_t SLANG_MCALL release() override {return 1;}
  SLANG_NO_THROW const void* SLANG_MCALL getBufferPointer() override {return data;}
  SLANG_NO_THROW std::size_t SLANG_MCALL getBufferSize() override {return size;}
};
bool equals(ISlangBlob* blob,const void* data,std::size_t size) {
  return blob && blob->getBufferSize()==size && std::memcmp(blob->getBufferPointer(),data,size)==0;
}
Slang::ComPtr<ISlangBlob> shader_key(slang::IGlobalSession* global,const std::filesystem::path& source) {
  const auto path=source.string();const char* search=path.c_str();
  slang::TargetDesc target{};target.format=SLANG_SPIRV;target.profile=global->findProfile("spirv_1_5");
  slang::SessionDesc desc{};desc.searchPaths=&search;desc.searchPathCount=1;desc.targets=&target;desc.targetCount=1;
  Slang::ComPtr<slang::ISession> session;require(SLANG_SUCCEEDED(global->createSession(desc,session.writeRef())),"Slang session");
  Slang::ComPtr<ISlangBlob> diagnostics;auto* module=session->loadModule("CacheMain",diagnostics.writeRef());
  require(module!=nullptr,"Slang module");
  Slang::ComPtr<slang::IEntryPoint> entry;require(SLANG_SUCCEEDED(module->findEntryPointByName("main",entry.writeRef())),"Slang entry");
  slang::IComponentType* components[]{module,entry};Slang::ComPtr<slang::IComponentType> composite,linked;
  require(SLANG_SUCCEEDED(session->createCompositeComponentType(components,2,composite.writeRef(),diagnostics.writeRef())),"Slang composite");
  require(SLANG_SUCCEEDED(composite->link(linked.writeRef(),diagnostics.writeRef())),"Slang link");
  Slang::ComPtr<ISlangBlob> key;linked->getEntryPointHash(0,0,key.writeRef());require(key && key->getBufferSize(),"SDK key");
  return key;
}
}
int main(int argc,char** argv) {
  if(argc==4 && std::strcmp(argv[1],"--cache-process")==0)return shader_cache_process_case(argv[2],argv[3]);
  require(argc==2,"fixture directory argument");const std::filesystem::path root=argv[1];
  require(!std::filesystem::exists(root),"fresh fixture directory");std::filesystem::create_directories(root);
  auto cache=create_shader_cache(root/"roundtrip");require(bool(cache),"cache creation");
  const std::array<std::uint8_t,4> key_bytes{1,2,3,4};View key(key_bytes.data(),key_bytes.size());
  std::array<std::uint8_t,4096> first{},second{};first.fill(7);second.fill(19);
  View a(first.data(),first.size()),b(second.data(),second.size());Slang::ComPtr<ISlangBlob> found,held;
  require(SLANG_FAILED(cache->queryCache(&key,found.writeRef())) && !found,"cold miss");
  require(SLANG_SUCCEEDED(cache->writeCache(&key,&a)),"write first");
  require(SLANG_SUCCEEDED(cache->queryCache(&key,held.writeRef())) && equals(held,first.data(),first.size()),"roundtrip");
  require(SLANG_SUCCEEDED(cache->writeCache(&key,&b)),"replace entry");cache.setNull();
  cache=create_shader_cache(root/"roundtrip");
  require(SLANG_SUCCEEDED(cache->queryCache(&key,found.writeRef())) && equals(found,second.data(),second.size()),"reopen persistence");
  require(equals(held,first.data(),first.size()),"returned blob survives replacement and cache lifetime");
  const auto file=root/"roundtrip/01020304.bin";
  {std::fstream out(file,std::ios::binary|std::ios::in|std::ios::out);out.seekp(-1,std::ios::end);out.put(0);}
  require(SLANG_FAILED(cache->queryCache(&key,found.writeRef())) && !found,"CRC corruption rejected");
  require(SLANG_SUCCEEDED(cache->writeCache(&key,&a)),"corrupt entry repair");
  std::filesystem::resize_file(file,12);
  require(SLANG_FAILED(cache->queryCache(&key,found.writeRef())) && !found,"truncated header rejected");
  require(SLANG_SUCCEEDED(cache->writeCache(&key,&b)),"truncated entry repair");
  {std::fstream out(file,std::ios::binary|std::ios::in|std::ios::out);const std::uint64_t huge=~std::uint64_t{};
    out.seekp(8);out.write(reinterpret_cast<const char*>(&huge),sizeof(huge));}
  require(SLANG_FAILED(cache->queryCache(&key,found.writeRef())) && !found,"oversized header rejected before allocation");
  View oversized(first.data(),64ull*1024*1024+1),long_key(first.data(),65);
  require(SLANG_FAILED(cache->writeCache(&key,&oversized)),"payload size bound");
  require(SLANG_FAILED(cache->writeCache(&long_key,&a)),"key size bound");
  require(SLANG_SUCCEEDED(cache->writeCache(&key,&a)),"restore before concurrency");
  std::atomic<unsigned> failures{};std::vector<std::thread> threads;
  for(unsigned worker=0;worker<4;++worker)threads.emplace_back([&,worker] {
    for(unsigned i=0;i<32;++i) {if(SLANG_FAILED(cache->writeCache(&key,(worker+i)%2?&a:&b)))++failures;
      Slang::ComPtr<ISlangBlob> read;if(SLANG_FAILED(cache->queryCache(&key,read.writeRef())) ||
        (!equals(read,first.data(),first.size()) && !equals(read,second.data(),second.size())))++failures;}
  });
  for(auto& thread:threads)thread.join();require(!failures,"concurrent SDK cache calls return complete independent blobs");
  for(const auto& entry:std::filesystem::directory_iterator(root/"roundtrip"))require(entry.path().extension()==".bin" || entry.path().filename()==".cache.lock","no leaked temporary files");
  Slang::ComPtr<slang::IGlobalSession> global;require(SLANG_SUCCEEDED(slang::createGlobalSession(global.writeRef())),"Slang global session");
  const auto shaders=root/"sources";std::filesystem::create_directory(shaders);
  {std::ofstream out(shaders/"CacheMain.slang");out<<"import CacheDependency; RWStructuredBuffer<uint> output; [shader(\"compute\")][numthreads(1,1,1)] void main(uint3 id:SV_DispatchThreadID) { output[id.x]=cache_value(); }";}
  {std::ofstream out(shaders/"CacheDependency.slang");out<<"uint cache_value() { return 7; }";}
  const auto original=shader_key(global,shaders);require(SLANG_SUCCEEDED(cache->writeCache(original,&a)),"write SDK dependency key");
  const auto unchanged=shader_key(global,shaders);require(equals(original,unchanged->getBufferPointer(),unchanged->getBufferSize()),"SDK key stable for unchanged source");
  require(SLANG_SUCCEEDED(cache->queryCache(unchanged,found.writeRef())),"unchanged shader hit");
  {std::ofstream out(shaders/"CacheDependency.slang");out<<"uint cache_value() { return 193; }";}
  const auto changed=shader_key(global,shaders);require(!equals(original,changed->getBufferPointer(),changed->getBufferSize()),"SDK key changes with imported source");
  require(SLANG_FAILED(cache->queryCache(changed,found.writeRef())) && !found,"source edit cannot reuse old shader blob");
  shader_cache_eviction_cases(root);
  std::printf("shader_cache_test=passed checks=%u concurrent_roundtrips=128 sdk_import_invalidation=1 gpu_runtime=0\n",checks);
}
