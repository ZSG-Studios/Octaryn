#pragma once
#include "BlockInteraction.h"
#include "BlockReceipts.h"
#include <filesystem>
#include <functional>

namespace octaryn::client::rendering {struct WorldCamera;}
namespace octaryn::client::app {
struct LocalPlayerInput;struct LocalPlayerPose;
class BlockActionsValidation {
public:
 using Submit=std::function<bool(const world_presentation::BlockEditIntent&,std::uint64_t*)>;
 ~BlockActionsValidation();
 void input(LocalPlayerInput&) const;
 void camera(rendering::WorldCamera&) const;
 void update(world_presentation::WorldStream&,world_presentation::BlockInteraction&,
  const LocalPlayerPose&,bool ready,double seconds,const Submit&);
 void receipt(const BlockReceipt&);
 void frame_rendered();
 bool capture_ready() const {return capture_ready_;}
 bool complete() const {return stage_==7;}
private:
 std::filesystem::path capture_,hold_;
 world_presentation::BlockPosition cell_{};
 std::uint16_t base_{};
 std::uint64_t command_{};
 unsigned stage_{},frames_{},stable_frames_{};
 bool initialized_{},selected_{},submitted_{},waiting_receipt_{},capture_ready_{};
 double started_{-1},last_submit_{},stable_since_{-1};
 const char* phase() const;
 std::filesystem::path sample() const;
};
}
