include_guard(GLOBAL)
include(Dependencies/GltfDependencies)
set(animation_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Animation")
octaryn_add_native_static_library(
    octaryn_client_animation
    client
    SOURCES
        "${animation_source}/Matrix.cpp"
        "${animation_source}/Pose.cpp"
        "${animation_source}/AssetImport.cpp"
        "${animation_source}/ClipImport.cpp"
        "${animation_source}/GeometryImport.cpp"
    PUBLIC_INCLUDE_DIRS "${animation_source}"
    PRIVATE_LINKS octaryn::deps::fastgltf)
add_executable(octaryn_animation_probe
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/AnimationProbe/Main.cpp")
target_link_libraries(octaryn_animation_probe PRIVATE octaryn_client_animation)
target_compile_features(octaryn_animation_probe PRIVATE cxx_std_20)
octaryn_owner_build_root(animation_tools_root tools)
set_target_properties(octaryn_animation_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${animation_tools_root}/animation")
add_dependencies(octaryn_client_native octaryn_client_animation)
octaryn_add_native_static_library(octaryn_client_animation_gpu client
    SOURCES "${animation_source}/DeformationGpu.cpp"
    PUBLIC_INCLUDE_DIRS "${animation_source}"
    PRIVATE_LINKS octaryn_client_animation octaryn_client_render_backend octaryn::deps::slang_rhi)
add_dependencies(octaryn_client_native octaryn_client_animation_gpu)
