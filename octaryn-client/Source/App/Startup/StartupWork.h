#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <string>

namespace octaryn::client::app {
// Single worker, one outstanding main-thread operation. No RHI objects are
// accessed concurrently: the worker sleeps until the main operation returns.
class StartupWork {
public:
  struct Cancelled {};

  void cancel() {cancelled_.store(true,std::memory_order_release);}
  bool cancelled() const {return cancelled_.load(std::memory_order_acquire);}

  static void progress(const char* stage, void* user) {
    auto& work=*static_cast<StartupWork*>(user);
    if(work.cancelled())throw Cancelled{};
    std::lock_guard lock(work.mutex_);
    work.stage_=stage;
  }

  static void main_thread(void (*operation)(void*), void* argument, void* user) {
    auto& work=*static_cast<StartupWork*>(user);
    std::unique_lock lock(work.mutex_);
    work.operation_=operation;
    work.argument_=argument;
    work.serviced_.wait(lock,[&work] {return !work.operation_;});
    if(work.operation_failure_) {
      auto failure=work.operation_failure_;
      work.operation_failure_=nullptr;
      std::rethrow_exception(failure);
    }
  }

  std::string pump() {
    std::unique_lock lock(mutex_);
    if(operation_) {
      auto callback=operation_;
      void* argument=argument_;
      lock.unlock();
      std::exception_ptr failure;
      try {callback(argument);} catch(...) {failure=std::current_exception();}
      lock.lock();
      operation_failure_=failure;
      operation_=nullptr;
      serviced_.notify_one();
    }
    return stage_;
  }

private:
  std::atomic<bool> cancelled_{};
  std::mutex mutex_;
  std::condition_variable serviced_;
  void (*operation_)(void*){};
  void* argument_{};
  std::exception_ptr operation_failure_;
  std::string stage_{"graphics device"};
};
}
