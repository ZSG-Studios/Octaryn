include_guard(GLOBAL)
include(Dependencies/GltfDependencies)
include(Owners/ContentDigestTargets)
octaryn_add_native_static_library(octaryn_gltf_buffers shared
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/Gltf/GltfBufferViews.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/Gltf/GltfMappedViews.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/Gltf/GltfAccessorBounds.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/Gltf/GltfTriangleReader.cpp"
    PUBLIC_INCLUDE_DIRS "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/Libraries/Gltf"
    PRIVATE_LINKS octaryn::deps::meshoptimizer)
target_link_libraries(octaryn_gltf_buffers PUBLIC octaryn::deps::fastgltf octaryn_content_digest)
