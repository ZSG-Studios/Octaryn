include_guard(GLOBAL)
include(Owners/SceneGeometryTargets)
include(Owners/SceneCollisionTargets)
set(geometry_runtime "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/VirtualGeometry")
target_sources(octaryn_client_render_backend PRIVATE
    "${geometry_runtime}/SelectionGpu.cpp"
    "${geometry_runtime}/SelectionResources.cpp"
    "${geometry_runtime}/SelectionResourcesFrame.cpp"
    "${geometry_runtime}/SceneRasterTables.cpp"
    "${geometry_runtime}/SceneRasterFrame.cpp"
    "${geometry_runtime}/SceneRasterWorld.cpp"
    "${geometry_runtime}/HybridRenderer.cpp"
    "${geometry_runtime}/OcclusionGpu.cpp"
    "${geometry_runtime}/RayGeometry.cpp"
    "${geometry_runtime}/RayGeometryBuild.cpp"
    "${geometry_runtime}/RayGeometryStages.cpp"
    "${geometry_runtime}/RayGeometryCompaction.cpp"
    "${geometry_runtime}/SceneRayScheduler.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/RenderBackend/WorldRaySceneMemory.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/Rendering/RenderBackend/WorldRaySceneAdmission.cpp"
    "${geometry_runtime}/WorldGeometry.cpp"
    "${geometry_runtime}/GeometryRootUpload.cpp"
    "${geometry_runtime}/MapGeometryCache.cpp"
    "${geometry_runtime}/WorldGeometryRaster.cpp"
    "${geometry_runtime}/WorldGeometryRay.cpp")
target_sources(octaryn_client_render_backend PRIVATE "${geometry_runtime}/SceneForward.cpp"
    "${geometry_runtime}/SceneAssets.cpp" "${geometry_runtime}/SceneSession.cpp" "${geometry_runtime}/SceneSessionJobs.cpp"
    "${geometry_runtime}/SceneAssetsHierarchy.cpp" "${geometry_runtime}/SceneAssetsResources.cpp"
    "${geometry_runtime}/SceneCut.cpp"
    "${geometry_runtime}/ScenePublication.cpp")
target_include_directories(octaryn_client_render_backend PUBLIC "${geometry_runtime}")
target_link_libraries(octaryn_client_render_backend PRIVATE
    octaryn_virtual_geometry_residency octaryn_virtual_geometry_stream octaryn_scene_preparation octaryn_scene_geometry octaryn_scene_collision)
