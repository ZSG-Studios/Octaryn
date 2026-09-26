#pragma once

namespace octaryn::client::app {

// RmlUi has process-global system/render interfaces; every UI owner (menu
// stack, debug overlay) acquires the runtime through this handle so the
// first owner initializes and the last one shuts down.
class RmlRuntimeHandle {
public:
    RmlRuntimeHandle();
    ~RmlRuntimeHandle();
    RmlRuntimeHandle(const RmlRuntimeHandle&) = delete;
    RmlRuntimeHandle& operator=(const RmlRuntimeHandle&) = delete;
};

}
