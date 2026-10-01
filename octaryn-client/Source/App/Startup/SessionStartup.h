#pragma once
#include "LocalSession.h"
#include "octaryn_native_schedule_runtime.h"
#include <atomic>
#include <filesystem>
#include <string>
#include <stdexcept>
#include <thread>
#include <chrono>

namespace octaryn::client::app {
// The session is exclusively worker-owned until ready() publishes completion.
class SessionStartup {
public:
  SessionStartup(LocalSession& session,std::filesystem::path bundle,std::filesystem::path world,
      unsigned radius,std::string endpoint,std::filesystem::path logs)
      :session_(session),bundle_(std::move(bundle)),world_(std::move(world)),logs_(std::move(logs)),
       endpoint_(std::move(endpoint)),radius_(radius) {
    scheduler_=octaryn_native_schedule_runtime_create(1,1);
    if(!scheduler_)throw std::runtime_error("World startup scheduler unavailable");
    octaryn_native_schedule_runtime_job job{};
    job.job_id="world_session_start";job.execute=execute;job.context=this;
    task_=octaryn_native_schedule_runtime_submit_worker(scheduler_,&job,1);
    if(!task_) {
      octaryn_native_schedule_runtime_destroy(scheduler_);scheduler_=nullptr;
      throw std::runtime_error("World startup job unavailable");
    }
  }
  ~SessionStartup() {
    if(!accepted_)cancel();
    if(task_)octaryn_native_schedule_runtime_task_destroy(task_);
    if(scheduler_)octaryn_native_schedule_runtime_destroy(scheduler_);
  }
  void cancel() {cancel_=true;decision_.store(-1,std::memory_order_release);}
  void accept() {accepted_=true;decision_.store(1,std::memory_order_release);}
  bool cancelling() const {return cancel_.load();}
  bool ready() const {return cancel_?octaryn_native_schedule_runtime_task_ready(task_)!=0:prepared_.load(std::memory_order_acquire);}
  bool succeeded() const {return ready() && started_ && !cancel_;}
  const std::string& error() const {return error_;}
private:
  static int execute(void* user) noexcept {
    auto& state=*static_cast<SessionStartup*>(user);
    try {
      if(!state.cancel_)state.started_=state.endpoint_.empty()
          ?state.session_.start(state.bundle_,state.world_,state.radius_,state.logs_)
          :state.session_.start_remote(state.bundle_,state.world_,state.radius_,state.endpoint_,state.logs_);
      state.error_=state.session_.status();
      if(state.started_) {
        state.prepared_.store(true,std::memory_order_release);
        while(!state.decision_.load(std::memory_order_acquire))std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if(state.decision_.load()!=1 || state.cancel_) {state.session_.stop();state.started_=false;}
      } else state.session_.stop();
    }catch(const std::exception& error) {
      state.error_=error.what();state.started_=false;
      try {state.session_.stop();}catch(...) {}
    }
    state.prepared_.store(true,std::memory_order_release);
    return state.started_?0:-1;
  }
  LocalSession& session_;
  std::filesystem::path bundle_,world_,logs_;
  std::string endpoint_,error_;
  unsigned radius_{};
  void* scheduler_{};
  void* task_{};
  std::atomic_bool cancel_{};
  std::atomic_bool prepared_{};
  std::atomic_int decision_{};
  bool started_{},accepted_{};
};
}
