#pragma once
#include "HostExports.h"
#include "Prediction.h"
#include "SessionIo.h"
#include <array>

namespace octaryn::client::app::local_session {
inline SessionChannels remote_channels() {
  return {
    [](uint8_t kind, std::string_view text) {
      return octaryn_client_remote_submit_intent(kind, text.data(), static_cast<int>(text.size())) == 0;
    },
    [](uint64_t& epoch, uint64_t& sequence) {
      return octaryn_client_remote_poll_action_ack(&epoch, &sequence) == 1;
    }
  };
}
inline void poll_remote_pose(SessionIo::Update& update) {
  static_assert(sizeof(octaryn_remote_pose) == 80);
  octaryn_remote_pose wire{};
  wire.version = 1;
  wire.size = sizeof(wire);
  if (octaryn_client_remote_poll_pose(&wire) != 1) return;
  LocalPlayerPose pose;
  pose.x = wire.x; pose.y = wire.y; pose.z = wire.z;
  pose.pitch = wire.pitch; pose.yaw = wire.yaw;
  pose.velocity_x = wire.velocity_x; pose.velocity_y = wire.velocity_y; pose.velocity_z = wire.velocity_z;
  pose.source_tick = wire.source_tick; pose.source_seconds = wire.source_seconds;
  pose.world_day_fraction = wire.world_day_fraction; pose.world_total_seconds = wire.world_total_seconds;
  pose.on_ground = (wire.flags & 1) != 0;
  pose.flying = (wire.flags & 2) != 0;
  pose.jump_held = (wire.flags & 4) != 0;
  update.pose = pose;
  update.acknowledged_input_frame = wire.acknowledged_input_frame;
}

inline bool submit_remote_commands(const PredictionPacket& packet) {
  static_assert(sizeof(octaryn_remote_command) == 40);
  std::array<octaryn_remote_command, 64> commands;
  if (packet.commands.empty() || packet.commands.size() > commands.size()) return false;
  for (size_t i = 0; i < packet.commands.size(); ++i) {
    const auto& from = packet.commands[i];
    commands[i] = {from.frameIndex, from.flags, from.controller, from.moveX,
                  from.moveY, from.moveZ, from.cameraPitch, from.cameraYaw, from.relativeMouse};
  }
  return octaryn_client_remote_submit_commands(commands.data(), static_cast<int>(packet.commands.size()),
                                              sizeof(octaryn_remote_command)) == 0;
}
}
