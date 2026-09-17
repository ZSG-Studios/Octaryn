#include "WorldItemsClient.h"
#include "ItemFiles.h"
#include "SessionIoWait.h"
#include <bit>
#include <cmath>
#include <limits>
#include <mutex>
#include <thread>

namespace octaryn::client::world_presentation {
namespace wi=octaryn::world_items;
struct WorldItemsClient::State {
  mutable std::mutex mutex;
  app::local_session::SessionIoWait wait;
  std::thread worker;
  std::filesystem::path pending_path,intent_path,snapshot_path;
  wi::Intent pending;
 wi::State received;
 std::optional<ProvisionalToss> provisional;
 std::chrono::steady_clock::time_point toss_started;
 std::uint64_t next_request{1};
  std::shared_ptr<const WorldItemSnapshot> visible=std::make_shared<WorldItemSnapshot>();
  std::string message="waiting_for_world_items";
  bool ready{},changed{},clear_pending{},stopping{};
  explicit State(const std::filesystem::path& root) {
    std::filesystem::create_directories(root/"client");
    std::filesystem::create_directories(root/"runtime");
    pending_path=root/"client/world_items.pending";intent_path=root/"runtime/world_items.intent";
    snapshot_path=root/"runtime/world_items.snapshot";
    if(std::filesystem::exists(pending_path)) {
      if(!item_files::read(pending_path,&pending,sizeof(pending))||pending.version!=1||pending.size!=sizeof(pending))
        throw std::runtime_error("invalid_pending_world_item_command");
      changed=true;
    }
    worker=std::thread([this]{run();});
  }
  ~State() {
    {std::lock_guard lock(mutex);stopping=true;}
    wait.stop();if(worker.joinable())worker.join();
  }
  static bool valid(const wi::State& s) {
    if(s.version!=1||s.size!=sizeof(s)||s.item_count>wi::max_items||s.grant_count>wi::max_grants||
        !std::isfinite(s.seconds)||s.seconds<0||static_cast<unsigned>(s.receipt_result)>4)return false;
    for(std::uint32_t n=0;n<s.item_count;++n) {
      const auto& i=s.items[n];
      if(!i.id||!i.block||i.block>65535||!i.count||i.count>wi::stack_limit||
        !std::isfinite(i.x)||!std::isfinite(i.y)||!std::isfinite(i.z))return false;
    }
    auto previous=s.acknowledged_grant;
    for(std::uint32_t n=0;n<s.grant_count;++n) {
      const auto& g=s.grants[n];
      if(g.id<=previous||!g.block||g.block>65535||!g.count||g.count>wi::stack_limit)return false;
      previous=g.id;
    }
    return true;
  }
  void run() {
    auto deadline=app::local_session::SessionIoWait::Clock::now();
    while(true) {
      wi::Intent outgoing;bool publish{},clear{};
      {std::lock_guard lock(mutex);if(stopping)break;outgoing=pending;publish=changed;clear=clear_pending;}
      try {
        if(publish) {
          // Durable outbox precedes command publication; retransmissions keep the same ID.
          item_files::write(pending_path,&outgoing,sizeof(outgoing));
          item_files::write(intent_path,&outgoing,sizeof(outgoing));
          std::lock_guard lock(mutex);
          if(pending.command==outgoing.command&&pending.acknowledge==outgoing.acknowledge)changed=false;
        }
        if(clear&&!publish) {
          std::filesystem::remove(pending_path);
          std::lock_guard lock(mutex);clear_pending=false;
        }
        wi::State next;
        if(item_files::read(snapshot_path,&next,sizeof(next))) {
          if(!valid(next)){std::lock_guard lock(mutex);message="invalid_world_items_snapshot";}
          else {
          auto snapshot=std::make_shared<WorldItemSnapshot>();snapshot->source_seconds=next.seconds;
          snapshot->items.assign(next.items,next.items+next.item_count);
 std::lock_guard lock(mutex);received=next;visible=std::move(snapshot);ready=true;message="world_items_ready";
 if(provisional&&next.last_command==provisional->command)provisional.reset();
          }
        }
      } catch(const std::exception& e) {std::lock_guard lock(mutex);message=e.what();}
      deadline=app::local_session::SessionIoWait::next(deadline,app::local_session::SessionIoWait::Clock::now());
      try {if(!wait.until(deadline))break;}
      catch(const std::exception& e){std::lock_guard lock(mutex);message=e.what();break;}
    }
  }
};
WorldItemsClient::WorldItemsClient(const std::filesystem::path& root):state_(std::make_unique<State>(root)) {
  static_assert(std::endian::native==std::endian::little);
}
WorldItemsClient::~WorldItemsClient()=default;
bool WorldItemsClient::submit_drop(std::uint16_t block,std::uint32_t count,const TossPose* pose) {
  auto& s=*state_;std::lock_guard lock(s.mutex);
  if(!s.ready||s.pending.command||!block||!count||count>wi::drop_limit||
      s.received.last_command==std::numeric_limits<std::uint64_t>::max())return false;
 s.pending.command=s.received.last_command+1;s.pending.block=block;s.pending.count=count;
 if(pose&&valid_toss_pose(*pose)) {
  s.provisional=make_provisional_toss(s.next_request++,s.pending.command,block,count,*pose);
  s.toss_started=std::chrono::steady_clock::now();
 }
 s.changed=true;s.clear_pending=false;return true;
}
WorldItemPresentation WorldItemsClient::presentation() const {
 auto& s=*state_;std::lock_guard lock(s.mutex);
 WorldItemPresentation result{s.visible,s.provisional};
 if(result.provisional) {
  const double age=std::chrono::duration<double>(std::chrono::steady_clock::now()-s.toss_started).count();
  if(age>=2)result.provisional.reset();
  else advance_provisional_toss(*result.provisional,age);
 }
 return result;
}
void WorldItemsClient::cancel_provisional() {
 auto& s=*state_;std::lock_guard lock(s.mutex);s.provisional.reset();
}
bool WorldItemsClient::drop_receipt(DropReceipt& receipt) const {
  auto& s=*state_;std::lock_guard lock(s.mutex);
  if(!s.pending.command||s.received.last_command!=s.pending.command)return false;
  receipt={s.pending.command,s.received.receipt_block,s.received.receipt_count,s.received.receipt_result};return true;
}
void WorldItemsClient::acknowledge_drop(std::uint64_t command) {
  auto& s=*state_;std::lock_guard lock(s.mutex);
  if(s.pending.command!=command||s.received.last_command!=command)return;
  s.pending.command=0;s.pending.block=0;s.pending.count=0;s.changed=true;s.clear_pending=true;
}
bool WorldItemsClient::next_pickup(PickupGrant& grant) const {
  auto& s=*state_;std::lock_guard lock(s.mutex);
  if(!s.received.grant_count||s.received.grants[0].id<=s.pending.acknowledge)return false;
  grant=s.received.grants[0];return true;
}
void WorldItemsClient::acknowledge_pickup(std::uint64_t grant) {
  auto& s=*state_;std::lock_guard lock(s.mutex);
  if(!s.received.grant_count||s.received.grants[0].id!=grant)return;
  s.pending.acknowledge=grant;s.changed=true;
}
std::shared_ptr<const WorldItemSnapshot> WorldItemsClient::snapshot() const {
  std::lock_guard lock(state_->mutex);return state_->visible;
}
std::string WorldItemsClient::status() const {std::lock_guard lock(state_->mutex);return state_->message;}
std::uint64_t WorldItemsClient::acknowledged_pickup() const {
  std::lock_guard lock(state_->mutex);return state_->received.acknowledged_grant;
}
}
