#include "RmlRuntime.h"

#include <RmlUi/Core.h>

namespace octaryn::client::app {
namespace {

int s_refCount = 0;
bool s_initialized = false;

} // namespace

RmlRuntimeHandle::RmlRuntimeHandle() {
    if (s_refCount++ == 0) {
        s_initialized = Rml::Initialise();
    }
}

RmlRuntimeHandle::~RmlRuntimeHandle() {
    if (--s_refCount == 0 && s_initialized) {
        Rml::Shutdown();
        s_initialized = false;
        Rml::SetSystemInterface(nullptr);
        Rml::SetRenderInterface(nullptr);
    }
}

}
