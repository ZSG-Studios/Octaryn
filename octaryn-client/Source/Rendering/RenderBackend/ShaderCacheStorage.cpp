#include "ShaderCacheStorage.h"
#include <chrono>
#include <cerrno>
#include <thread>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace octaryn::client::rendering::shader_cache {
bool key_name(const std::filesystem::path& path) {
  const auto name=path.filename().native();
  if(name.size()<6 || name.size()>132 || !name.ends_with(std::filesystem::path(".bin").native()) || (name.size()-4)%2)return false;
  for(std::size_t i=0;i<name.size()-4;++i)
    if(!((name[i]>='0' && name[i]<='9') || (name[i]>='a' && name[i]<='f')))return false;
  return true;
}
bool regular_entry(const std::filesystem::path& path) {
  std::error_code error;
  if(!key_name(path) || !std::filesystem::is_regular_file(std::filesystem::symlink_status(path,error)) || error)return false;
#if defined(_WIN32)
  WIN32_FIND_DATAW found{};const auto search=FindFirstFileW(path.c_str(),&found);
  if(search==INVALID_HANDLE_VALUE)return false;
  FindClose(search);
  return !(found.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && path.filename().native()==found.cFileName;
#else
  return true;
#endif
}
bool inventory(const std::filesystem::path& directory,Inventory& result) {
  result={};std::error_code error;
  std::filesystem::directory_iterator iterator(directory,error),end;
  if(error)return false;
  for(;iterator!=end;iterator.increment(error)) {
    if(error)return false;
    const auto path=iterator->path();if(!key_name(path))continue;
    const auto status=iterator->symlink_status(error);if(error)return false;
    if(!std::filesystem::is_regular_file(status) || !regular_entry(path))continue;
    const auto bytes=iterator->file_size(error);if(error)return false;
    const auto used=iterator->last_write_time(error);if(error)return false;
    result.files.push_back({path,bytes,used});result.bytes+=bytes;
  }
  return !error;
}
DirectoryLock::DirectoryLock(const std::filesystem::path& directory) {
  const auto path=directory/".cache.lock";
#if defined(_WIN32)
  const auto file=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
      nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
  if(file==INVALID_HANDLE_VALUE)return;
  BY_HANDLE_FILE_INFORMATION info{};
  if(!GetFileInformationByHandle(file,&info) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) {
    CloseHandle(file);return;
  }
  handle=reinterpret_cast<std::intptr_t>(file);
#else
  const auto file=open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
  if(file<0)return;
  struct stat info{};
  if(fstat(file,&info)!=0 || !S_ISREG(info.st_mode)) {close(file);return;}
  handle=file;
#endif
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(25);
  do {
#if defined(_WIN32)
    OVERLAPPED offset{};
    held=LockFileEx(file,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&offset)!=0;
    if(!held && GetLastError()!=ERROR_LOCK_VIOLATION)break;
#else
    held=flock(file,LOCK_EX|LOCK_NB)==0;
    if(!held && errno!=EWOULDBLOCK && errno!=EAGAIN)break;
#endif
    if(held)break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while(std::chrono::steady_clock::now()<deadline);
}
DirectoryLock::~DirectoryLock() {
  if(handle==-1)return;
#if defined(_WIN32)
  const auto file=reinterpret_cast<HANDLE>(handle);
  if(held) {OVERLAPPED offset{};UnlockFileEx(file,0,1,0,&offset);}
  CloseHandle(file);
#else
  const auto file=static_cast<int>(handle);if(held)flock(file,LOCK_UN);close(file);
#endif
}
Temporary::~Temporary() {std::error_code error;std::filesystem::remove(path,error);}
bool publish(const std::filesystem::path& temporary,const std::filesystem::path& destination) {
#if defined(_WIN32)
  return MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
  std::error_code error;std::filesystem::rename(temporary,destination,error);return !error;
#endif
}
}
