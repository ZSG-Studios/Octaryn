#include "world/edit/internal.h"

namespace world_edit_internal {

// The actual bookkeeping work lives in hidden_blocks.cpp and traces.cpp.
// This translation unit remains as the shared bookkeeping anchor for the
// module-level build grouping.
[[maybe_unused]] constexpr int kBookkeepingModuleAnchor = 0;

} // namespace world_edit_internal
