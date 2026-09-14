#pragma once
namespace octaryn::client::app {
enum class CameraShoulder { Left, Right };
constexpr CameraShoulder opposite_shoulder(CameraShoulder value) {
  return value==CameraShoulder::Right?CameraShoulder::Left:CameraShoulder::Right;
}
}
