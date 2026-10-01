#pragma once
namespace octaryn::character_motion {
inline constexpr float WalkSpeedBlocksPerSecond=5.f;
inline constexpr float SprintWalkSpeedBlocksPerSecond=9.f;
inline constexpr float NormalFlySpeedBlocksPerSecond=10.f;
inline constexpr float SprintFlySpeedBlocksPerSecond=100.f;
inline constexpr float MinimumFlyHeight=-1000.f,MaximumFlyHeight=1000.f;
inline constexpr float JumpSpeed=8.f;
inline constexpr float Gravity=24.f;
inline constexpr float StepUpHeight=.3f;
inline constexpr float StepDownDepth=.2f;
}
