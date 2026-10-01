include_guard(GLOBAL)
octaryn_add_native_static_library(octaryn_content_digest shared
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/Content/ResourceDigest.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/Content/ResourceTreeDigest.cpp"
    PUBLIC_INCLUDE_DIRS "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/Content")
