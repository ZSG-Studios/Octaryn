#pragma once

#include <cstdio>

namespace octaryn::client::app {

constexpr unsigned VoxelRuntimeFrames = 3u;

bool run_frame_loop(FILE *log);

} // namespace octaryn::client::app
