octaryn_add_native_static_library(
    octaryn_client_threading
    client
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Threading/BackgroundThread.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Threading/ThreadCpuTime.cpp")
add_dependencies(octaryn_client_native octaryn_client_threading)
