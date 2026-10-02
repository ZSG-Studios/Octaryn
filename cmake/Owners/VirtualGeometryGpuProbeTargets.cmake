include_guard(GLOBAL)
set(geometry_gpu_probe "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/VirtualGeometryGpuProbe")
add_executable(octaryn_virtual_geometry_gpu_probe "${geometry_gpu_probe}/main.cpp"
    "${geometry_gpu_probe}/SelectionProbe.cpp" "${geometry_gpu_probe}/HybridProbe.cpp")
target_sources(octaryn_virtual_geometry_gpu_probe PRIVATE "${geometry_gpu_probe}/InstanceSelectionGpuProbe.cpp")
target_sources(octaryn_virtual_geometry_gpu_probe PRIVATE "${geometry_gpu_probe}/SharedSelectionProbe.cpp"
    "${geometry_gpu_probe}/ScenePoolProbe.cpp")
target_sources(octaryn_virtual_geometry_gpu_probe PRIVATE "${geometry_gpu_probe}/SceneRasterProbe.cpp")
target_sources(octaryn_virtual_geometry_gpu_probe PRIVATE "${geometry_gpu_probe}/AnimationGpuProbe.cpp")
target_sources(octaryn_virtual_geometry_gpu_probe PRIVATE "${geometry_gpu_probe}/RayGeometryProbe.cpp")
target_sources(octaryn_virtual_geometry_gpu_probe PRIVATE "${geometry_gpu_probe}/MapRayMaterialProbe.cpp")
target_sources(octaryn_virtual_geometry_gpu_probe PRIVATE "${geometry_gpu_probe}/OcclusionBinProbe.cpp")
target_compile_features(octaryn_virtual_geometry_gpu_probe PRIVATE cxx_std_20)
target_link_libraries(octaryn_virtual_geometry_gpu_probe PRIVATE octaryn_client_render_backend
    octaryn_virtual_geometry_residency octaryn_client_animation_gpu octaryn::deps::slang_rhi)
target_compile_definitions(octaryn_virtual_geometry_gpu_probe PRIVATE
    OCTARYN_GEOMETRY_PROBE_SHADER="${geometry_gpu_probe}/Probe.slang"
    OCTARYN_RAY_PROBE_SHADER="${geometry_gpu_probe}/RayProbe.slang"
    OCTARYN_MAP_RAY_PROBE_SHADER="${geometry_gpu_probe}/MapRayProbe.slang"
    OCTARYN_GEOMETRY_SHADER_DIR="${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/VirtualGeometry")
target_compile_definitions(octaryn_virtual_geometry_gpu_probe PRIVATE
    OCTARYN_ANIMATION_SHADER="${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Shaders/Animation/Deform.slang")
octaryn_owner_build_root(geometry_gpu_probe_root tools)
set_target_properties(octaryn_virtual_geometry_gpu_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${geometry_gpu_probe_root}/virtual-geometry")
if(WIN32)
    add_custom_command(TARGET octaryn_virtual_geometry_gpu_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:octaryn_native_jobs>" "$<TARGET_FILE_DIR:octaryn_virtual_geometry_gpu_probe>"
        VERBATIM)
endif()
