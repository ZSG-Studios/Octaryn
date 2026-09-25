#pragma once
#include <array>
#include <cstddef>
#include <span>

namespace octaryn::client::app {
enum class BlockActionKind {Cycle,Pick,Break,Place,Select};
struct BlockAction {
  BlockActionKind kind{};
  int delta{};
};
class BlockActionQueue {
public:
  static constexpr std::size_t capacity=64;
  void clear() {size_=0;overflow_=false;edit_requested_=false;}
  bool push(BlockAction action) {
    if(action.kind==BlockActionKind::Cycle && action.delta==0) return true;
    edit_requested_|=action.kind==BlockActionKind::Break || action.kind==BlockActionKind::Place;
    if(size_==capacity) {overflow_=true;return false;}
    actions_[size_++]=action;
    return true;
  }
  std::span<const BlockAction> actions() const {return {actions_.data(),size_};}
  bool overflowed() const {return overflow_;}
  bool edit_requested() const {return edit_requested_;}
private:
  std::array<BlockAction,capacity> actions_{};
  std::size_t size_{};
  bool overflow_{},edit_requested_{};
};
}
