#pragma once

namespace octaryn::client::threading {
// Dedicated CPU workers yield scheduling priority to presentation on Windows.
void set_background_thread_priority(const char* role);
}
