include_guard(GLOBAL)
include(Owners/VirtualGeometryCookTargets)
include(Owners/VirtualGeometryResidencyTargets)
set(geometry_stream_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/VirtualGeometry")
add_library(octaryn_scene_memory STATIC "${geometry_stream_source}/SceneMemoryLedger.cpp")
target_compile_features(octaryn_scene_memory PUBLIC cxx_std_20)
target_include_directories(octaryn_scene_memory PUBLIC "${geometry_stream_source}")
add_library(octaryn_virtual_geometry_stream STATIC
    "${geometry_stream_source}/GeometryStream.cpp"
    "${geometry_stream_source}/GeometryStreamJobs.cpp"
    "${geometry_stream_source}/SceneGeometryPool.cpp"
    "${geometry_stream_source}/SceneGeometryPoolFrame.cpp"
    "${geometry_stream_source}/SceneGeometryPoolJobs.cpp")
target_compile_features(octaryn_virtual_geometry_stream PUBLIC cxx_std_20)
target_include_directories(octaryn_virtual_geometry_stream PUBLIC "${geometry_stream_source}")
target_link_libraries(octaryn_virtual_geometry_stream PUBLIC octaryn_virtual_geometry_asset
    octaryn_virtual_geometry_residency octaryn_scene_memory octaryn_native_jobs octaryn::deps::slang_rhi)
add_executable(octaryn_virtual_geometry_stream_probe
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/VirtualGeometryCook/StreamProbe.cpp")
target_link_libraries(octaryn_virtual_geometry_stream_probe PRIVATE octaryn_virtual_geometry_stream)
target_include_directories(octaryn_virtual_geometry_stream_probe PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
octaryn_owner_build_root(geometry_stream_tools tools)
set_target_properties(octaryn_virtual_geometry_stream_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${geometry_stream_tools}/virtual-geometry")
if(WIN32)
    add_custom_command(TARGET octaryn_virtual_geometry_stream_probe POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:octaryn_native_jobs>" "$<TARGET_FILE_DIR:octaryn_virtual_geometry_stream_probe>"
        VERBATIM)
endif()
add_executable(octaryn_scene_memory_probe "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/VirtualGeometryProbe/SceneMemoryProbe.cpp")
target_link_libraries(octaryn_scene_memory_probe PRIVATE octaryn_scene_memory)
set_target_properties(octaryn_scene_memory_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${geometry_stream_tools}/virtual-geometry")
