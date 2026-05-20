#include "WorldMeshBatchBudget.h"

#include <algorithm>
#include <cstdlib>

namespace octaryn_client_app {

size_t server_stream_mesh_batch_budget() {
  constexpr size_t kDefaultBudget = 16u;
  constexpr size_t kMaxBudget = 128u;
  const char *value =
      std::getenv("OCTARYN_CLIENT_SERVER_STREAM_MESH_COLUMNS_PER_FRAME");
  if (value == nullptr || value[0] == '\0') {
    return kDefaultBudget;
  }
  const long parsed = std::strtol(value, nullptr, 10);
  return std::clamp(parsed <= 0 ? kDefaultBudget : static_cast<size_t>(parsed),
                    size_t{1u}, kMaxBudget);
}

} // namespace octaryn_client_app
