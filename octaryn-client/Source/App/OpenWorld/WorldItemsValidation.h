#pragma once
#include "LocalSession.h"
#include "WorldItemWire.h"
#include <cstdint>
namespace octaryn::client::rendering {struct WorldCamera;}
namespace octaryn::client::world_presentation {class WorldItemsClient;}
namespace octaryn::client::app {
class GameUi;
// Explicit CLI qualification through production domain APIs; no synthesized OS events.
class WorldItemsValidation {
public:
  void observe(GameUi&,world_presentation::WorldItemsClient&,const LocalPlayerPose&,bool ready,double seconds);
  void input(LocalPlayerInput&,const LocalPlayerPose&) const;
  void camera(rendering::WorldCamera&) const;
  void frame_rendered(bool captured);
  bool capture_ready() const {return phase_==Phase::Visible&&target_.age>=.5;}
  bool complete() const {return phase_==Phase::Complete;}
private:
  enum class Phase {Start,Drop,Visible,Pickup,Complete};
  Phase phase_{Phase::Start};
  std::uint16_t block_{};
  std::uint32_t original_count_{};
  std::uint32_t requested_count_{1},expected_entities_{1},grant_count_{},grant_total_{},credited_inventory_{};
  std::uint64_t original_drop_{},original_grant_{};
  std::uint64_t first_item_{},last_grant_{};
  octaryn::world_items::Item target_{};
  double started_{-1},phase_started_{};
  bool seen_item_{};
};
}
