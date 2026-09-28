#pragma once
#include "FrameFenceRecord.h"
#include <slang.h>
namespace octaryn::client::rendering {
// Same retirement state machine for runtime fences and focused callback fixtures.
template<class Current,class Wait,class Clock>
FrameFenceRecord retire_frame_fence(unsigned slot,std::uint64_t value,std::uint64_t source_frame,
    Current current,Wait wait,Clock clock) {
  FrameFenceRecord r;r.slot=slot;r.value=value;r.source_frame=value?source_frame:UINT64_MAX;
  r.times.fill(clock());
  if(!value) {r.success=true;return r;}
  r.result=current(r.before);r.times[1]=r.times[2]=r.times[3]=clock();
  if(SLANG_FAILED(r.result) || r.before==UINT64_MAX) {
    if(SLANG_SUCCEEDED(r.result))r.result=SLANG_FAIL;return r;
  }
  if(r.before<value) {
    r.waited=true;r.result=wait();r.times[2]=r.times[3]=clock();
    if(SLANG_FAILED(r.result))return r;
    r.result=current(r.after);r.times[3]=clock();
    if(SLANG_FAILED(r.result) || r.after==UINT64_MAX || r.after<value) {
      if(SLANG_SUCCEEDED(r.result))r.result=SLANG_FAIL;return r;
    }
  } else r.after=r.before;
  r.success=true;return r;
}
}
