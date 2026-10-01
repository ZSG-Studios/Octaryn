#pragma once
#include <utility>
namespace octaryn::client::rendering {
// Independent queue work needs an owner fence even if acquisition/encoding fails.
template<class Retire,class Failed>
class FrameSubmissionGuard {
  const bool& started_;
  Retire retire_;
  Failed failed_;
  bool owned_{};
public:
  FrameSubmissionGuard(const bool& started,Retire retire,Failed failed):
      started_(started),retire_(std::move(retire)),failed_(std::move(failed)) {}
  FrameSubmissionGuard(const FrameSubmissionGuard&)=delete;
  FrameSubmissionGuard& operator=(const FrameSubmissionGuard&)=delete;
  ~FrameSubmissionGuard() {
    if(started_ && !owned_ && !retire_())failed_();
  }
  // A successful same-queue submission's slot fence now protects the queries.
  void submitted() {owned_=true;}
};
}
