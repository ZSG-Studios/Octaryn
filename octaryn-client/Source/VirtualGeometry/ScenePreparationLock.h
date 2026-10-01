#pragma once
#include "FilePath.h"
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// A process crash releases the lock; interrupted catalogs remain resumable.
class ScenePreparationLock {
public:
  explicit ScenePreparationLock(const std::filesystem::path& catalog) {
    if(!catalog.parent_path().empty())std::filesystem::create_directories(octaryn::content::file_io_path(catalog.parent_path()));
    auto lock=catalog;lock+=".lock";
#ifdef _WIN32
    handle_=CreateFileW(octaryn::content::file_io_path(lock).c_str(),GENERIC_READ|GENERIC_WRITE|DELETE,0,nullptr,OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
    if(handle_==INVALID_HANDLE_VALUE)throw std::runtime_error("scene catalog is locked or its directory is not writable");
#else
    handle_=::open(lock.c_str(),O_CREAT|O_RDWR,0666);
    if(handle_<0)throw std::runtime_error("scene catalog lock could not be opened");
    if(flock(handle_,LOCK_EX|LOCK_NB)!=0) {::close(handle_);handle_=-1;throw std::runtime_error("scene catalog is already being prepared");}
#endif
  }
  ~ScenePreparationLock() {
#ifdef _WIN32
    if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);
#else
    if(handle_>=0)::close(handle_);
#endif
  }
  ScenePreparationLock(const ScenePreparationLock&)=delete;
  ScenePreparationLock& operator=(const ScenePreparationLock&)=delete;
private:
#ifdef _WIN32
  HANDLE handle_{INVALID_HANDLE_VALUE};
#else
  int handle_{-1};
#endif
};
