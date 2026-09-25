#include "ItemFiles.h"
#include <fstream>
#include <stdexcept>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace octaryn::client::world_presentation::item_files {
bool read(const std::filesystem::path& path,void* bytes,std::size_t size) {
#if defined(_WIN32)
  const auto handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
    nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(handle==INVALID_HANDLE_VALUE)return false;
  LARGE_INTEGER length{};DWORD count{};
  const bool result=GetFileSizeEx(handle,&length)&&length.QuadPart==static_cast<LONGLONG>(size)&&
    ReadFile(handle,bytes,static_cast<DWORD>(size),&count,nullptr)&&count==size;
  CloseHandle(handle);return result;
#else
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  if(!file||file.tellg()!=static_cast<std::streamoff>(size))return false;
  file.seekg(0);return static_cast<bool>(file.read(static_cast<char*>(bytes),static_cast<std::streamsize>(size)));
#endif
}
void write(const std::filesystem::path& path,const void* bytes,std::size_t size) {
  auto temporary=path;temporary+=".tmp";
#if defined(_WIN32)
  const auto file=CreateFileW(temporary.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("world_items_write_open_failed");
  DWORD written{};
  const bool okay=WriteFile(file,bytes,static_cast<DWORD>(size),&written,nullptr)&&written==size&&FlushFileBuffers(file);
  CloseHandle(file);
  if(!okay||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    throw std::runtime_error("world_items_atomic_write_failed");
#else
  {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
   if(!file.write(static_cast<const char*>(bytes),static_cast<std::streamsize>(size))||!file.flush())
     throw std::runtime_error("world_items_write_failed");}
  std::filesystem::rename(temporary,path);
#endif
}
}
