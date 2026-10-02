#include "SceneLoading.h"
#include "ScenePreparation.h"
#include <cstring>
#include <exception>
#include <algorithm>
#include <stdexcept>

extern "C" int octaryn_scene_loading_verify(const char* root,const char* descriptor,char* error,uint32_t capacity) {
  if(!root || !descriptor || !error || capacity<1024)return -1;
  error[0]=0;
  try {
    const auto bounded=[](const char* value) {
      std::size_t size=0;while(size<4096 && value[size])++size;
      if(!size || size==4096)throw std::runtime_error("Scene verification path exceeds admission");
      return std::filesystem::path(reinterpret_cast<const char8_t*>(value),reinterpret_cast<const char8_t*>(value+size));
    };
    if(!octaryn::scene_loading::reserve_verification_work())
      throw std::runtime_error("Scene verification working budget unavailable");
    struct Lease {~Lease(){octaryn::scene_loading::release_verification_work();}} lease;
    octaryn::scene_loading::Preparation work;
    work.root=std::filesystem::canonical(bounded(root));work.input=bounded(descriptor);
    work.progress=[](std::uint64_t,std::uint64_t) {};
    auto snapshot=octaryn::scene_loading::prepare(work);
    return snapshot?0:-1;
  }catch(const std::exception& failure) {
    const auto size=std::min<std::size_t>(std::strlen(failure.what()),capacity-1);
    std::memcpy(error,failure.what(),size);error[size]=0;return -1;
  }catch(...) {std::strcpy(error,"Scene verification failed");return -1;}
}
