#pragma once
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <span>
#include <string>
#include <algorithm>

namespace octaryn::content {
// CNG selects the OS implementation, including hardware SHA instructions.
class PlatformResourceHash {
  struct Api {
    HMODULE module{LoadLibraryW(L"bcrypt.dll")};
    decltype(&BCryptOpenAlgorithmProvider) open{};
    decltype(&BCryptCloseAlgorithmProvider) close{};
    decltype(&BCryptCreateHash) create{};
    decltype(&BCryptDestroyHash) destroy{};
    decltype(&BCryptHashData) update{};
    decltype(&BCryptFinishHash) finish{};
    BCRYPT_ALG_HANDLE algorithm{};
    Api() {
      if(!module)return;
      open=reinterpret_cast<decltype(open)>(GetProcAddress(module,"BCryptOpenAlgorithmProvider"));
      close=reinterpret_cast<decltype(close)>(GetProcAddress(module,"BCryptCloseAlgorithmProvider"));
      create=reinterpret_cast<decltype(create)>(GetProcAddress(module,"BCryptCreateHash"));
      destroy=reinterpret_cast<decltype(destroy)>(GetProcAddress(module,"BCryptDestroyHash"));
      update=reinterpret_cast<decltype(update)>(GetProcAddress(module,"BCryptHashData"));
      finish=reinterpret_cast<decltype(finish)>(GetProcAddress(module,"BCryptFinishHash"));
      if(open && close && create && destroy && update && finish)
        open(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    }
    ~Api() {if(algorithm)close(algorithm,0);if(module)FreeLibrary(module);}
  };
  static Api& api() {static Api value;return value;}
  BCRYPT_HASH_HANDLE hash_{};
public:
  PlatformResourceHash() {
    auto& a=api();if(a.algorithm)a.create(a.algorithm,&hash_,nullptr,0,nullptr,0,0);
  }
  ~PlatformResourceHash() {if(hash_)api().destroy(hash_);}
  PlatformResourceHash(const PlatformResourceHash&)=delete;
  PlatformResourceHash& operator=(const PlatformResourceHash&)=delete;
  explicit operator bool() const {return hash_!=nullptr;}
  bool append(std::span<const std::uint8_t> bytes) {
    if(!hash_)return false;
    while(!bytes.empty()) {
      const auto size=static_cast<ULONG>(std::min<std::size_t>(bytes.size(),1u<<20));
      if(api().update(hash_,const_cast<PUCHAR>(bytes.data()),size,0)<0)return false;
      bytes=bytes.subspan(size);
    }
    return true;
  }
  std::string finish() {
    std::array<unsigned char,32> bytes{};
    if(!hash_ || api().finish(hash_,bytes.data(),static_cast<ULONG>(bytes.size()),0)<0)return {};
    constexpr char digits[]="0123456789abcdef";
    std::string result;result.reserve(64);
    for(const auto value:bytes) {result+=digits[value>>4];result+=digits[value&15];}
    return result;
  }
};
}
#endif
