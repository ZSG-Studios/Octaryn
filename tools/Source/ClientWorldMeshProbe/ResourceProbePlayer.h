#pragma once
#include "Probe.h"
#include "AssetPath.h"
#include "SlangShaderPath.h"
#include "FrameWatchdog.h"
#include <memory>
namespace mesh_probe {
// Prepare shader compilation before continuous completed-frame supervision.
class ResourceProbePlayer {
  WorldRenderer& renderer;
  std::unique_ptr<PlayerRenderer,decltype(&destroy_player_renderer)> player{nullptr,destroy_player_renderer};
public:
  explicit ResourceProbePlayer(WorldRenderer& r):renderer(r) {
    char asset[4096]{};
    require(bundle_path_build(asset,sizeof(asset),"Client/Assets/Player/octaryn_player_v1.gltf"),"resource probe player asset");
    const auto shader=resolve_slang_shader_path("octaryn-client/Shaders/Player/Player.slang");
    player.reset(create_player_renderer(r.device,rhi::Format::RGBA16Float,rhi::Format::D32Float,asset,shader.c_str()));
    require(bool(player),"resource probe prepared player");
    for(unsigned slot=0;slot<2;++slot) {
      auto commands=r.queue->createCommandEncoder();require(bool(commands),"player setup encoder");
      require(prepare_player_shadows(player.get(),commands,slot,PlayerPose{},true),"player setup shadow pipeline");
      auto submission=commands->finish();require(bool(submission),"player setup commands");
      checked(r.queue->submit(submission),"player setup submit");
      if(!r.frame_queue.synchronize(r.queue,2000))frame_gpu_shutdown_failed("resource_probe_player_setup");
    }
  }
  ~ResourceProbePlayer() {
    if(renderer.player==player.get())renderer.player=nullptr;
    if(!renderer.frame_queue.synchronize(renderer.queue,2000))frame_gpu_shutdown_failed("resource_probe_player");
  }
  PlayerRenderer* get()const {return player.get();}
};
}
