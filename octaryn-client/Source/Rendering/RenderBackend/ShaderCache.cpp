#include "ShaderCache.h"
#include "ShaderCacheStorage.h"
#include "RayDiagnosticMode.h"
#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace octaryn::client::rendering {
namespace {
using namespace shader_cache;
class Blob final:public ISlangBlob {
  std::atomic<unsigned> refs{1};
public:
  std::vector<std::uint8_t> bytes;
  explicit Blob(std::size_t size):bytes(size) {}
  SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID& id,void** out) override {
    if(!out)return SLANG_E_INVALID_ARG;*out=nullptr;
    if(id!=ISlangBlob::getTypeGuid() && id!=ISlangUnknown::getTypeGuid())return SLANG_E_NO_INTERFACE;
    *out=static_cast<ISlangBlob*>(this);addRef();return SLANG_OK;
  }
  SLANG_NO_THROW std::uint32_t SLANG_MCALL addRef() override {return ++refs;}
  SLANG_NO_THROW std::uint32_t SLANG_MCALL release() override {const auto count=--refs;if(!count)delete this;return count;}
  SLANG_NO_THROW const void* SLANG_MCALL getBufferPointer() override {return bytes.data();}
  SLANG_NO_THROW std::size_t SLANG_MCALL getBufferSize() override {return bytes.size();}
};
std::string filename(ISlangBlob* key) {
  if(!key || !key->getBufferSize() || key->getBufferSize()>64)return {};
  static constexpr char hex[]="0123456789abcdef";
  std::string result;result.reserve(key->getBufferSize()*2+4);
  const auto* bytes=static_cast<const std::uint8_t*>(key->getBufferPointer());
  for(std::size_t i=0;i<key->getBufferSize();++i) {result+=hex[bytes[i]>>4];result+=hex[bytes[i]&15];}
  return result+".bin";
}
class Cache final:public rhi::IPersistentCache {
  std::atomic<unsigned> refs{1};
  std::mutex mutex;
  std::filesystem::path directory;
  std::uint64_t stored_bytes{},hits{},misses{},writes{},rejected{},evictions{},evicted_bytes{};
  std::uint64_t write_refused{},io_errors{},lock_unavailable{};
  std::size_t entries{};
  bool entries_fresh{};
  std::string process;
  void refresh(const Inventory& state) {stored_bytes=state.bytes;entries=state.files.size();entries_fresh=true;}
  bool refresh() {
    Inventory state;
    if(!inventory(directory,state)) {++io_errors;entries_fresh=false;return false;}
    refresh(state);return true;
  }
  bool make_room(Inventory& state,const std::filesystem::path& target,bool existed,std::uint64_t old_bytes,
      std::uint64_t new_bytes) {
    std::sort(state.files.begin(),state.files.end(),[](const Entry& a,const Entry& b) {
      return a.used!=b.used?a.used<b.used:a.path.native()<b.path.native();
    });
    const auto fits=[&] {return entries+(existed?0u:1u)<=MaxEntries && stored_bytes-old_bytes+new_bytes<=MaxBytes;};
    for(const auto& victim:state.files) {
      if(fits())break;
      if(victim.path==target || !regular_entry(victim.path))continue;
      std::error_code error;
      // Recheck observed metadata before deleting an exact direct cache entry.
      if(std::filesystem::file_size(victim.path,error)!=victim.bytes || error)continue;
      if(std::filesystem::last_write_time(victim.path,error)!=victim.used || error)continue;
      if(!std::filesystem::remove(victim.path,error) || error) {++io_errors;continue;}
      --entries;stored_bytes-=victim.bytes;++evictions;evicted_bytes+=victim.bytes;
    }
    return fits();
  }
public:
  explicit Cache(std::filesystem::path path):directory(std::filesystem::absolute(path).lexically_normal()) {
#if defined(_WIN32)
    process=std::to_string(GetCurrentProcessId());
#else
    process=std::to_string(getpid());
#endif
    std::error_code error;std::filesystem::create_directories(directory,error);
    if(error || !std::filesystem::is_directory(std::filesystem::symlink_status(directory)))
      throw std::filesystem::filesystem_error("shader cache directory",directory,error);
    DirectoryLock ownership(directory);
    if(ownership)refresh();else ++lock_unavailable;
  }
  ~Cache() {
    try {DirectoryLock ownership(directory);if(ownership)refresh();else {++lock_unavailable;entries_fresh=false;}}
    catch(...) {++io_errors;entries_fresh=false;}
    std::printf("shader_cache_summary kind=%s hits=%llu misses=%llu writes=%llu rejected=%llu bytes=%llu entries=%zu "
        "evictions=%llu evicted_bytes=%llu write_refused=%llu io_errors=%llu lock_unavailable=%llu entries_fresh=%u\n",
      directory.filename().string().c_str(),static_cast<unsigned long long>(hits),static_cast<unsigned long long>(misses),
      static_cast<unsigned long long>(writes),static_cast<unsigned long long>(rejected),
      static_cast<unsigned long long>(stored_bytes),entries,static_cast<unsigned long long>(evictions),
      static_cast<unsigned long long>(evicted_bytes),static_cast<unsigned long long>(write_refused),
      static_cast<unsigned long long>(io_errors),static_cast<unsigned long long>(lock_unavailable),unsigned(entries_fresh));
  }
  SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID& id,void** out) override {
    if(!out)return SLANG_E_INVALID_ARG;*out=nullptr;
    if(id!=rhi::IPersistentCache::getTypeGuid() && id!=ISlangUnknown::getTypeGuid())return SLANG_E_NO_INTERFACE;
    *out=static_cast<rhi::IPersistentCache*>(this);addRef();return SLANG_OK;
  }
  SLANG_NO_THROW std::uint32_t SLANG_MCALL addRef() override {return ++refs;}
  SLANG_NO_THROW std::uint32_t SLANG_MCALL release() override {const auto count=--refs;if(!count)delete this;return count;}
  SLANG_NO_THROW SlangResult SLANG_MCALL queryCache(ISlangBlob* key,ISlangBlob** out) override {
    if(!out)return SLANG_E_INVALID_ARG;*out=nullptr;
    try {
      std::lock_guard lock(mutex);
      try {
      const auto name=filename(key);if(name.empty())return SLANG_E_INVALID_ARG;
      DirectoryLock ownership(directory);
      if(!ownership) {++lock_unavailable;++misses;return SLANG_E_NOT_FOUND;}
      const auto path=directory/name;
      if(!regular_entry(path)) {++misses;return SLANG_E_NOT_FOUND;}
      std::ifstream file(path,std::ios::binary);
      if(!file) {++misses;++io_errors;return SLANG_E_NOT_FOUND;}
      std::array<std::uint64_t,3> header{};file.read(reinterpret_cast<char*>(header.data()),sizeof(header));
      std::error_code error;const auto size=std::filesystem::file_size(path,error);
      if(!file || error || header[0]!=Magic || !header[1] || header[1]>MaxBlob || size!=sizeof(header)+header[1]) {
        ++misses;++rejected;return SLANG_E_NOT_FOUND;
      }
      Slang::ComPtr<Blob> blob;blob.attach(new Blob(static_cast<std::size_t>(header[1])));
      file.read(reinterpret_cast<char*>(blob->bytes.data()),static_cast<std::streamsize>(blob->bytes.size()));
      if(!file || SDL_crc32(0,blob->bytes.data(),blob->bytes.size())!=header[2]) {++misses;++rejected;return SLANG_E_NOT_FOUND;}
      file.close();
      // Only validated hits become recent. Returned blobs own independent bytes.
      std::filesystem::last_write_time(path,std::filesystem::file_time_type::clock::now(),error);
      if(error)++io_errors;
      ++hits;*out=blob.detach();return SLANG_OK;
      } catch(...) {++misses;++io_errors;return SLANG_E_NOT_FOUND;}
    } catch(...) {return SLANG_E_NOT_FOUND;}
  }
  SLANG_NO_THROW SlangResult SLANG_MCALL writeCache(ISlangBlob* key,ISlangBlob* data) override {
    try {
      std::lock_guard lock(mutex);
      const auto refuse=[&](SlangResult result) {++write_refused;return result;};
      try {
      if(!data || !data->getBufferSize() || data->getBufferSize()>MaxBlob)return refuse(SLANG_E_INVALID_ARG);
      const auto name=filename(key);if(name.empty())return refuse(SLANG_E_INVALID_ARG);
      DirectoryLock ownership(directory);
      if(!ownership) {++lock_unavailable;return refuse(SLANG_E_NOT_AVAILABLE);}
      Inventory state;
      if(!inventory(directory,state)) {++io_errors;entries_fresh=false;return refuse(SLANG_FAIL);}
      refresh(state);const auto path=directory/name;
      std::error_code error;const auto status=std::filesystem::symlink_status(path,error);
      const bool existed=std::filesystem::exists(status);
      if((error && error!=std::errc::no_such_file_or_directory) || (existed && !regular_entry(path))) {
        ++io_errors;return refuse(SLANG_FAIL);
      }
      const auto prior=std::find_if(state.files.begin(),state.files.end(),[&](const Entry& entry){return entry.path==path;});
      const std::uint64_t old_bytes=prior==state.files.end()?0:prior->bytes;
      const auto bytes=data->getBufferSize()+3*sizeof(std::uint64_t);
      static std::atomic<std::uint64_t> serial{};
      const auto temporary_path=directory/(name+"."+process+"."+std::to_string(SDL_GetTicksNS())+"."+
          std::to_string(++serial)+".tmp");
      error.clear();
      if(std::filesystem::exists(temporary_path,error) || error) {++io_errors;return refuse(SLANG_FAIL);}
      Temporary temporary{temporary_path};
      const std::array<std::uint64_t,3> header{Magic,data->getBufferSize(),SDL_crc32(0,data->getBufferPointer(),data->getBufferSize())};
      std::ofstream file(temporary.path,std::ios::binary|std::ios::trunc);
      file.write(reinterpret_cast<const char*>(header.data()),sizeof(header));
      file.write(static_cast<const char*>(data->getBufferPointer()),static_cast<std::streamsize>(data->getBufferSize()));
      file.close();if(!file) {++io_errors;return refuse(SLANG_FAIL);}
      // The complete closed replacement exists before any old entry is evicted.
      if(!make_room(state,path,existed,old_bytes,bytes))return refuse(SLANG_E_NOT_AVAILABLE);
      error.clear();const auto final_status=std::filesystem::symlink_status(path,error);
      if((error && error!=std::errc::no_such_file_or_directory) ||
          (std::filesystem::exists(final_status) && !regular_entry(path)) || !publish(temporary.path,path)) {
        ++io_errors;entries_fresh=false;return refuse(SLANG_FAIL);
      }
      stored_bytes=stored_bytes-old_bytes+bytes;if(!existed)++entries;++writes;
      refresh();return SLANG_OK;
      } catch(...) {++io_errors;entries_fresh=false;return refuse(SLANG_FAIL);}
    } catch(...) {return SLANG_FAIL;}
  }
};
}
Slang::ComPtr<rhi::IPersistentCache> create_shader_cache(const std::filesystem::path& directory) {
  Slang::ComPtr<rhi::IPersistentCache> cache;
  try {cache.attach(new Cache(directory));} catch(...) {}
  return cache;
}
ShaderCaches configure_shader_caches(rhi::DeviceDesc& desc,bool ray_counters,unsigned reflection_wave,bool wave_telemetry) {
  ShaderCaches result;
  std::filesystem::path directory;
  const char* override_path=SDL_getenv("OCTARYN_CLIENT_SHADER_CACHE_PATH");
  if(override_path && *override_path) {
    directory=std::filesystem::path(reinterpret_cast<const char8_t*>(override_path));
    std::puts("shader_cache explicit_directory=1");
  } else {
    char* pref=SDL_GetPrefPath("ZSGStudios","Octaryn");
    if(!pref)return result;
    directory=std::filesystem::path(reinterpret_cast<const char8_t*>(pref))/"shader-cache";SDL_free(pref);
  }
  std::string tag=spGetBuildTagString();
  for(auto& c:tag)if(!std::isalnum(static_cast<unsigned char>(c)) && c!='.' && c!='-' && c!='_')c='_';
  const auto version="rhi-v1-"+tag+"-"+std::to_string(sizeof(void*)*8)+
      (std::endian::native==std::endian::little?"-le-":"-be-")+std::to_string(static_cast<unsigned>(desc.deviceType))+
      "-"+ray_counter_cache_variant(ray_counters)+"-reflectionwave"+std::to_string(reflection_wave)+
      "-waveobserve"+std::to_string(unsigned(wave_telemetry));
  result.shaders=create_shader_cache(directory/version/"shaders");result.pipelines=create_shader_cache(directory/version/"pipelines");
  desc.persistentShaderCache=result.shaders;desc.persistentPipelineCache=result.pipelines;
  desc.enableCompilationReports=true;
  std::printf("shader_cache sdk_keys=1 atomic=1 crc=1 max_entries=%zu max_bytes=%llu shaders=%u pipelines=%u namespace=%s eviction=recently_used lock_wait_ms=25\n",
      MaxEntries,static_cast<unsigned long long>(MaxBytes),result.shaders?1u:0u,result.pipelines?1u:0u,version.c_str());
  return result;
}
}
