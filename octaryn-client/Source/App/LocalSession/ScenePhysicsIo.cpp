#include "ScenePhysicsIo.h"
#include "SessionFiles.h"
#include <glaze/glaze.hpp>
#include <chrono>
#include <vector>

namespace octaryn::client::app::local_session {
struct Acknowledgement { int Version{}; uint64_t Epoch{}, Seq{}; };
struct Journal { int Version{1}; uint64_t Epoch{}, Seq{}; std::vector<glz::raw_json> Commands; };
bool ScenePhysicsIo::submit(std::string request) {
  if(request.empty() || request.size()>4096)return false;
  std::lock_guard lock(mutex_);
  if(pending_.size()>=256)return false;
  if(!epoch_)epoch_=uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
  pending_.emplace_back(++sequence_,std::move(request));
  return true;
}
bool ScenePhysicsIo::snapshot(std::string& text) const {
  std::lock_guard lock(mutex_);
  if(snapshot_.empty())return false;
  text=snapshot_;return true;
}
void ScenePhysicsIo::exchange(const std::filesystem::path& directory) {
  std::string incoming;
  Acknowledgement ack;
  constexpr glz::opts options{.error_on_unknown_keys=false};
  const bool received=read_text(directory/"scene_physics.snapshot.json",incoming,4*1024*1024) &&
    !glz::read<options>(ack,incoming) && ack.Version==1;
  Journal journal;
  {
    std::lock_guard lock(mutex_);
    if(received && ack.Epoch==epoch_ && ack.Seq<=sequence_) {
      while(!pending_.empty() && pending_.front().first<=ack.Seq)pending_.pop_front();
      snapshot_=std::move(incoming);
    }
    if(!epoch_)return;
    journal.Epoch=epoch_;journal.Seq=sequence_;
    for(const auto& entry:pending_)journal.Commands.push_back(glz::raw_json{entry.second});
  }
  std::string outgoing;
  if(!glz::write_json(journal,outgoing) && outgoing!=published_ &&
     write_text(directory/"scene_physics.commands.json",outgoing))published_=std::move(outgoing);
}
}
