#pragma once
#include <cstdint>

namespace octaryn::player_prediction {
inline constexpr uint32_t InputVersion = 2;
inline constexpr uint32_t RemoteVersion = 4;
inline constexpr double FixedDelta = 1.0 / 60.0;
inline constexpr uint32_t MaxBatch = 64;
inline constexpr uint32_t MaxOutstanding = 256;
inline constexpr uint32_t MaxCatchUpSteps = 8;
inline constexpr double StaleSeconds = 0.25;

// JSON names follow prediction-command-contract.md; this is not a packed packet.
struct Command {
    uint64_t frameIndex{};
    uint32_t flags{}, controller{};
    float moveX{}, moveY{}, moveZ{}, cameraPitch{}, cameraYaw{};
    int32_t relativeMouse{};
};
struct State {
    uint64_t acknowledgedInputFrame{}, simulationTick{};
    double simulationTime{};
    float x{}, y{}, z{}, pitch{}, yaw{};
    float velocityX{}, velocityY{}, velocityZ{};
    uint32_t controlMode{};
    bool grounded{}, jumpHeld{};
};
}
