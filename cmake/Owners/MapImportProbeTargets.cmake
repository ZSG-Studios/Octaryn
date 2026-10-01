include_guard(GLOBAL)
include(Owners/GltfBufferTargets)
set(map_import_world "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
octaryn_add_native_executable(octaryn_map_import_probe tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/MapImportProbe/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/MapImportProbe/Test.cpp"
        "${map_import_world}/MapModel.cpp" "${map_import_world}/MapSource.cpp" "${map_import_world}/MapMaterials.cpp"
        "${map_import_world}/MapSourceReader.cpp"
    PUBLIC_INCLUDE_DIRS "${map_import_world}"
    PRIVATE_LINKS octaryn_gltf_buffers octaryn::deps::meshoptimizer)
