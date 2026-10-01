#include "ShaderCache.h"
#include "ShaderCacheStorage.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>
using namespace octaryn::client::rendering;
namespace {
void check(bool ok,const char* label) {
  if(!ok) {std::fprintf(stderr,"shader_cache_eviction_test failed: %s\n",label);std::exit(1);}
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
std::array<std::uint8_t,4> key_bytes(unsigned value) {
  return {static_cast<std::uint8_t>(value>>24),static_cast<std::uint8_t>(value>>16),
      static_cast<std::uint8_t>(value>>8),static_cast<std::uint8_t>(value)};
}
std::filesystem::path key_path(const std::filesystem::path& directory,unsigned value) {
  char name[16]{};std::snprintf(name,sizeof(name),"%08x.bin",value);return directory/name;
}
SlangResult write(rhi::IPersistentCache* cache,unsigned value,const void* data,std::size_t size) {
  const auto bytes=key_bytes(value);View key(bytes.data(),bytes.size()),blob(data,size);
  return cache->writeCache(&key,&blob);
}
Slang::ComPtr<ISlangBlob> read(rhi::IPersistentCache* cache,unsigned value) {
  const auto bytes=key_bytes(value);View key(bytes.data(),bytes.size());Slang::ComPtr<ISlangBlob> result;
  cache->queryCache(&key,result.writeRef());return result;
}
bool equal(ISlangBlob* blob,const void* data,std::size_t size) {
  return blob && blob->getBufferSize()==size && std::memcmp(blob->getBufferPointer(),data,size)==0;
}
void age(const std::filesystem::path& directory,unsigned count) {
  const auto start=std::filesystem::file_time_type::clock::now()-std::chrono::hours(24);
  for(unsigned i=0;i<count;++i)std::filesystem::last_write_time(key_path(directory,i),start+std::chrono::seconds(i));
}
std::pair<std::size_t,std::uint64_t> sizes(const std::filesystem::path& directory) {
  std::size_t count{};std::uint64_t bytes{};
  for(const auto& entry:std::filesystem::directory_iterator(directory))
    if(entry.is_regular_file() && entry.path().extension()==".bin") {++count;bytes+=entry.file_size();}
  return {count,bytes};
}
bool retry_write(rhi::IPersistentCache* cache,unsigned key,const void* data,std::size_t size) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  do {
    if(SLANG_SUCCEEDED(write(cache,key,data,size)))return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  } while(std::chrono::steady_clock::now()<deadline);
  return false;
}
Slang::ComPtr<ISlangBlob> retry_read(rhi::IPersistentCache* cache,unsigned key) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  do {
    auto result=read(cache,key);if(result)return result;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  } while(std::chrono::steady_clock::now()<deadline);
  return {};
}
void capacity(const std::filesystem::path& directory) {
  auto cache=create_shader_cache(directory);check(bool(cache),"capacity cache creation");
  std::array<std::uint8_t,256> first{},second{};first.fill(7);second.fill(19);
  for(unsigned i=0;i<512;++i)check(SLANG_SUCCEEDED(write(cache,i,first.data(),first.size())),"fill all 512 entries");
  age(directory,512);const auto held=read(cache,0);
  check(equal(held,first.data(),first.size()),"hold and refresh oldest valid key");
  check(SLANG_SUCCEEDED(write(cache,512,first.data(),first.size())),"new key admitted at entry capacity");
  check(sizes(directory).first==512 && !read(cache,1) && read(cache,0),"oldest unused key evicted; recent key retained");
  check(SLANG_SUCCEEDED(write(cache,511,second.data(),second.size())),"replacement at capacity");
  check(sizes(directory).first==512 && read(cache,2) && equal(read(cache,511),second.data(),second.size()),
      "capacity replacement did not evict another key");
  // Make the held key oldest again; eviction must not alter its returned blob.
  std::filesystem::last_write_time(key_path(directory,0),std::filesystem::file_time_type::clock::now()-std::chrono::hours(48));
  check(SLANG_SUCCEEDED(write(cache,513,first.data(),first.size())) && !read(cache,0),"evict externally held key");
  check(equal(held,first.data(),first.size()),"held blob survived eviction");
  cache.setNull();cache=create_shader_cache(directory);
  check(equal(read(cache,512),first.data(),first.size()) && equal(read(cache,513),first.data(),first.size()),
      "new capacity-admitted keys persisted after reopening");
  {std::fstream file(key_path(directory,511),std::ios::binary|std::ios::in|std::ios::out);file.seekp(-1,std::ios::end);file.put(0);}
  check(!read(cache,511) && SLANG_SUCCEEDED(write(cache,511,first.data(),first.size())) &&
      equal(read(cache,511),first.data(),first.size()) && sizes(directory).first==512,"corrupt capacity entry repaired without eviction");
  const auto foreign=directory/"foreign.tmp";{std::ofstream file(foreign);file<<"preserve";}
  const auto nested=directory/"00112233.bin";std::filesystem::create_directory(nested);
  {std::ofstream file(nested/"save");file<<"preserve";}
  {std::ofstream file(directory/"ABCDEF01.bin");file<<"preserve";}
  const auto unicode=directory/std::filesystem::path(u8"\u96ea.bin");
  {std::ofstream file(unicode);file<<"preserve";}
  {std::ofstream file(directory/"abc.bin");file<<"preserve";}
#if defined(_WIN32)
  check(!read(cache,0xabcdef01u) && SLANG_FAILED(write(cache,0xabcdef01u,first.data(),first.size())),
      "case-insensitive foreign filename cannot be read or replaced");
#endif
  check(SLANG_SUCCEEDED(write(cache,514,first.data(),first.size())) && std::filesystem::exists(foreign) &&
      std::filesystem::exists(nested/"save") && std::filesystem::file_size(directory/"ABCDEF01.bin")==8 &&
      std::filesystem::file_size(unicode)==8 && std::filesystem::file_size(directory/"abc.bin")==8,
      "eviction preserved foreign temporary, directory and non-key file");
  const auto outside=directory.parent_path()/"external-blob";
  std::filesystem::copy_file(key_path(directory,514),outside);
  std::error_code error;std::filesystem::create_symlink(outside,key_path(directory,515),error);
  if(!error) {
    const auto bytes=std::filesystem::file_size(outside);
    check(!read(cache,515) && SLANG_FAILED(write(cache,515,first.data(),first.size())) &&
        SLANG_SUCCEEDED(write(cache,516,first.data(),first.size())) && std::filesystem::is_symlink(key_path(directory,515)) &&
        std::filesystem::file_size(outside)==bytes,"linked entry is neither read, replaced nor evicted");
    std::puts("shader_cache_symlink=passed");
  } else std::printf("shader_cache_symlink=unavailable error_code=%d category=%s\n",error.value(),error.category().name());
  for(const auto& entry:std::filesystem::directory_iterator(directory))
    check(entry.path().extension()!=std::filesystem::path(".tmp") || entry.path()==foreign,"owned write temporary cleaned");
}
void byte_capacity(const std::filesystem::path& directory) {
  auto cache=create_shader_cache(directory);check(bool(cache),"byte cache creation");
  std::vector<std::uint8_t> payload(64u*1024*1024,31);
  for(unsigned i=0;i<3;++i)check(SLANG_SUCCEEDED(write(cache,i,payload.data(),payload.size())),"maximum blob admission");
  // Four payloads plus four 24-byte headers total exactly 256 MiB.
  check(SLANG_SUCCEEDED(write(cache,3,payload.data(),payload.size()-96)),"fill exact byte bound including headers");
  check(sizes(directory)==std::pair<std::size_t,std::uint64_t>{4,256ull*1024*1024},"exact byte capacity reached");
  age(directory,4);const auto held=read(cache,0);check(bool(held),"refresh recent maximum-sized blob");
  const std::uint64_t value=123;
  check(SLANG_SUCCEEDED(write(cache,4,&value,sizeof(value))) && !read(cache,1) && read(cache,0),
      "byte pressure evicts the oldest unused entry");
  check(sizes(directory).first==4 && sizes(directory).second<=256ull*1024*1024,"byte pressure retained bounded storage");
  check(equal(held,payload.data(),payload.size()),"maximum held blob survived byte eviction");
  check(SLANG_SUCCEEDED(write(cache,0,&value,sizeof(value))) && sizes(directory).first==4 && read(cache,2),
      "shrinking replacement subtracts actual previous bytes without eviction");
  cache.setNull();cache=create_shader_cache(directory);
  check(equal(read(cache,4),&value,sizeof(value)),"byte-pressure admitted key persists");
}
void concurrent(const std::filesystem::path& directory) {
  std::array<std::uint8_t,4096> first{},second{};first.fill(11);second.fill(29);
  auto initial=create_shader_cache(directory);
  check(SLANG_SUCCEEDED(write(initial,0,first.data(),first.size())),"concurrent fixture seed");
  const auto held=read(initial,0);std::atomic<unsigned> failures{};
  std::vector<std::thread> workers;
  for(unsigned worker=0;worker<4;++worker)workers.emplace_back([&,worker] {
    auto cache=create_shader_cache(directory);if(!cache) {++failures;return;}
    for(unsigned iteration=0;iteration<32;++iteration) {
      const auto& data=(worker+iteration)%2?first:second;
      if(!retry_write(cache,0,data.data(),data.size()))++failures;
      const auto blob=retry_read(cache,0);
      if(!equal(blob,first.data(),first.size()) && !equal(blob,second.data(),second.size()))++failures;
    }
  });
  for(auto& worker:workers)worker.join();
  check(!failures && equal(held,first.data(),first.size()),"independent instances/readers preserve atomic and held blobs");
  {
    shader_cache::DirectoryLock lock(directory);check(bool(lock),"qualification lock acquired");
    const auto before=sizes(directory);const auto start=std::chrono::steady_clock::now();
    check(SLANG_FAILED(write(initial,1,first.data(),first.size())) && !read(initial,0),"busy lock refuses unlocked fallback");
    check(std::chrono::steady_clock::now()-start<std::chrono::seconds(1) && sizes(directory)==before,
        "contended admission remains bounded and does not alter storage");
  }
  check(equal(read(initial,0),first.data(),first.size()) || equal(read(initial,0),second.data(),second.size()),
      "cache resumes after contended ownership releases");
}
}
void shader_cache_eviction_cases(const std::filesystem::path& root) {
  capacity(root/"capacity");byte_capacity(root/"bytes");concurrent(root/"instances");
  auto cache=create_shader_cache(root/"processes");const unsigned value=17;
  for(unsigned i=0;i<500;++i)check(SLANG_SUCCEEDED(write(cache,i,&value,sizeof(value))),"multiprocess capacity seed");
  std::puts("shader_cache_eviction=passed capacity=512 byte_budget=268435456 headers=1 recent_preserved=1 "
      "reopen=1 capacity_replacement=1 corruption_repair=1 held_blobs=1 independent_instances=4 lock_refusal=1");
}
int shader_cache_process_case(const std::filesystem::path& directory,const char* mode) {
  auto cache=create_shader_cache(directory);check(bool(cache),"process cache open");
  if(std::strcmp(mode,"inspect")==0) {
    check(sizes(directory).first==512 && sizes(directory).second<=256ull*1024*1024,"process capacity bound");
    for(unsigned base:{4096u,8192u})for(unsigned i=0;i<32;++i) {
      const unsigned expected=base+i;check(equal(read(cache,expected),&expected,sizeof(expected)),"both processes persisted their new keys");
    }
    std::puts("shader_cache_processes=passed writers=2 new_keys=64 entries=512");return 0;
  }
  const unsigned base=static_cast<unsigned>(std::strtoul(mode,nullptr,10));check(base==4096 || base==8192,"known process key range");
  std::vector<Slang::ComPtr<ISlangBlob>> held;
  for(unsigned i=0;i<32;++i) {
    const unsigned value=base+i;
    check(retry_write(cache,value,&value,sizeof(value)),"process write with bounded contention retry");
    held.push_back(retry_read(cache,value));check(equal(held.back(),&value,sizeof(value)),"process immediate independent blob");
  }
  for(unsigned i=0;i<32;++i) {const unsigned value=base+i;check(equal(held[i],&value,sizeof(value)),"process held blob");}
  std::printf("shader_cache_process_writer=passed first_key=%u count=32\n",base);return 0;
}
