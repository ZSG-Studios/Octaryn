#pragma once
#include "octaryn_host_api.h"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

namespace octaryn::client::host {
struct SceneTransitionRequest {
  uint64_t revision{};
  std::string asset_id;
  std::filesystem::path descriptor_path;
  octaryn_host_transition_pose pose{};
  uint64_t queued_at_ns{};
};
// One main-thread mailbox. Revisions survive module/session teardown so the
// app can complete a replacement after shutting down the previous host.
struct SceneTransitionMailbox {
  octaryn_host_transition_view view{};
  SceneTransitionRequest request;
  uint64_t next{},revision{};
  uint32_t state=OCTARYN_TRANSITION_FAILED;
  std::string error;
};
inline SceneTransitionMailbox scene_transition_mailbox;
inline bool transition_pose_valid(const octaryn_host_transition_pose& p) {
  return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<=1000000&&
    std::abs(p.y)<=1000000&&std::abs(p.z)<=1000000&&std::isfinite(p.yaw)&&std::isfinite(p.pitch)&&
    std::abs(p.yaw)<=1000000&&std::abs(p.pitch)<=1.55f;
}
inline bool transition_text(const char* value,size_t bound,std::string& result) {
  if(!value)return false;size_t size=0;while(size<bound && value[size])++size;
  if(!size || size==bound)return false;result.assign(value,size);
  return result.find_first_of("\r\n\t")==std::string::npos;
}
inline void scene_transition_publish_view(const std::string& asset_id,const octaryn_host_transition_pose& pose,bool enabled) {
  if(!SDL_IsMainThread())return;
  auto& v=scene_transition_mailbox.view;v={};
  if(asset_id.empty() || asset_id.size()>=sizeof(v.asset_id) || !transition_pose_valid(pose))return;
  v.pose=pose;v.enabled=enabled?1u:0u;std::memcpy(v.asset_id,asset_id.c_str(),asset_id.size()+1);
}
inline int scene_transition_read(octaryn_host_transition_view* output) {
  if(!SDL_IsMainThread() || !output || !scene_transition_mailbox.view.asset_id[0])return -1;
  *output=scene_transition_mailbox.view;return 0;
}
inline int scene_transition_begin(const char* asset,const char* descriptor,const octaryn_host_transition_pose* pose,uint64_t* revision) {
  if(!SDL_IsMainThread() || !pose || !revision || !transition_pose_valid(*pose))return -1;
  auto& m=scene_transition_mailbox;
  if(!m.view.enabled || m.state==OCTARYN_TRANSITION_QUEUED || m.state==OCTARYN_TRANSITION_LOADING || m.next==UINT64_MAX)return -1;
  std::string id,path;if(!transition_text(asset,256,id) || !transition_text(descriptor,4096,path))return -1;
  auto source=std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str()));if(!source.is_absolute())return -1;
  m.revision=++m.next;m.request={m.revision,std::move(id),std::move(source),*pose,SDL_GetTicksNS()};
  m.error.clear();m.state=OCTARYN_TRANSITION_QUEUED;*revision=m.revision;return 0;
}
inline bool scene_transition_take(SceneTransitionRequest& output) {
  auto& m=scene_transition_mailbox;if(!SDL_IsMainThread() || m.state!=OCTARYN_TRANSITION_QUEUED)return false;
  output=m.request;m.state=OCTARYN_TRANSITION_LOADING;return true;
}
inline void scene_transition_complete(uint64_t revision,bool success,const std::string& error) {
  auto& m=scene_transition_mailbox;
  if(!SDL_IsMainThread() || revision!=m.revision || m.state!=OCTARYN_TRANSITION_LOADING)return;
  m.state=success?OCTARYN_TRANSITION_COMPLETED:OCTARYN_TRANSITION_FAILED;m.error=error.substr(0,1023);m.request={};
}
inline int scene_transition_status(uint64_t revision,uint32_t* state,char* error,uint32_t capacity) {
  const auto& m=scene_transition_mailbox;
  if(!SDL_IsMainThread() || !revision || revision!=m.revision || !state || !error || capacity<1024)return -1;
  *state=m.state;std::memcpy(error,m.error.c_str(),m.error.size()+1);return 0;
}
inline int scene_transition_cancel(uint64_t revision) {
  auto& m=scene_transition_mailbox;
  if(!SDL_IsMainThread() || revision!=m.revision || m.state!=OCTARYN_TRANSITION_QUEUED)return -1;
  m.state=OCTARYN_TRANSITION_FAILED;m.error="Queued transition canceled.";m.request={};return 0;
}
inline void scene_transition_reset() {if(SDL_IsMainThread()) {const auto next=scene_transition_mailbox.next;scene_transition_mailbox={};scene_transition_mailbox.next=next;}}
}
