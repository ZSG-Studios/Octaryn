#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <ostream>
#include <streambuf>
#include <thread>
#ifdef OCTARYN_PROFILE_TESTS
#include <functional>
#endif

namespace octaryn::client::diagnostics {
// Bounded lossless profiling. Saturation fails the capture instead of stalling
// rendering or silently discarding the slow frames we need to diagnose.
class AsyncProfileBuffer final : public std::streambuf {
#ifdef OCTARYN_PROFILE_TESTS
  friend struct AsyncProfileTestAccess;
  std::function<void()> before_write_,before_close_;
#endif
  static constexpr std::size_t Capacity=64,ChunkSize=16384;
  struct Chunk {std::array<char,ChunkSize> bytes;std::size_t size{};};
  std::unique_ptr<std::array<Chunk,Capacity>> queue_;
  std::array<char,ChunkSize> staging_;
  std::ofstream file_;
  std::thread worker_;
  std::mutex mutex_;
  std::condition_variable ready_;
  std::size_t head_{},count_{};
  bool closing_{};
  std::atomic<bool> failed_{};
  void fail() {
    if(!failed_.exchange(true))std::fputs("profile_writer_failed capture_invalid=1\n",stderr);
  }
  void write() noexcept {
    Chunk chunk;
    for(;;) {
      {
        std::unique_lock lock(mutex_);
        ready_.wait(lock,[&]{return count_ || closing_;});
        if(!count_)break;
        const auto& source=(*queue_)[head_];chunk.size=source.size;
        std::memcpy(chunk.bytes.data(),source.bytes.data(),chunk.size);
        head_=(head_+1)%Capacity;--count_;
      }
#ifdef OCTARYN_PROFILE_TESTS
      if(before_write_)before_write_();
#endif
      file_.write(chunk.bytes.data(),std::streamsize(chunk.size));file_.flush();
      if(!file_)fail();
    }
  }
protected:
  int sync() override {
    if(failed_)return -1;
    const auto size=std::size_t(pptr()-pbase());
    if(!size)return 0;
    {
      std::lock_guard lock(mutex_);
      if(closing_ || count_==Capacity || !queue_) {fail();return -1;}
      auto& target=(*queue_)[(head_+count_)%Capacity];target.size=size;
      std::memcpy(target.bytes.data(),pbase(),size);++count_;
      setp(staging_.data(),staging_.data()+staging_.size());
    }
    ready_.notify_one();return 0;
  }
  int_type overflow(int_type value) override {
    if(sync()!=0)return traits_type::eof();
    if(!traits_type::eq_int_type(value,traits_type::eof())) {
      *pptr()=traits_type::to_char_type(value);pbump(1);
    }
    return traits_type::not_eof(value);
  }
public:
  AsyncProfileBuffer() {setp(staging_.data(),staging_.data()+staging_.size());}
  ~AsyncProfileBuffer() override {close();}
  bool open(const std::filesystem::path& path) {
    if(worker_.joinable())return false;
    file_.open(path,std::ios::binary);if(!file_)return false;
    queue_=std::make_unique<std::array<Chunk,Capacity>>();
    worker_=std::thread([this]{write();});return true;
  }
  bool is_open() const {return file_.is_open();}
  bool healthy() const {return !failed_.load(std::memory_order_acquire);}
  bool close() {
    if(!worker_.joinable())return !failed_;
    sync();
    {std::lock_guard lock(mutex_);closing_=true;}
    ready_.notify_one();worker_.join();
#ifdef OCTARYN_PROFILE_TESTS
    if(before_close_)before_close_();
#endif
    file_.close();if(!file_)fail();return !failed_;
  }
};
class AsyncProfileStream final : public std::ostream {
#ifdef OCTARYN_PROFILE_TESTS
  friend struct AsyncProfileTestAccess;
#endif
  AsyncProfileBuffer buffer_;
public:
  AsyncProfileStream():std::ostream(nullptr) {rdbuf(&buffer_);}
  void open(const std::filesystem::path& path) {if(!buffer_.open(path))setstate(std::ios::failbit);}
  bool is_open() const {return buffer_.is_open();}
  explicit operator bool() const {return std::ostream::operator bool() && buffer_.healthy();}
  bool operator!() const {return !static_cast<bool>(*this);}
  bool close() {const bool ok=buffer_.close();if(!ok)setstate(std::ios::badbit);return ok;}
};
}
