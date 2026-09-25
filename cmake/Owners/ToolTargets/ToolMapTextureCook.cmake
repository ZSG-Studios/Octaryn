include_guard(GLOBAL)
include(Dependencies/MapTextureCodec)
set(map_texture_source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/MapWorld")
octaryn_add_native_executable(octaryn_map_texture_cook tools
    SOURCES
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/MapTextureCook/main.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/MapTextureCook/Encode.cpp"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/Source/MapTextureCook/Test.cpp"
        "${map_texture_source}/MapModel.cpp"
        "${map_texture_source}/MapMaterials.cpp"
        "${map_texture_source}/MapImages.cpp"
        "${map_texture_source}/MapMipmaps.cpp"
        "${map_texture_source}/MapTextureCache.cpp"
        "${map_texture_source}/MapTextureHash.cpp"
    PUBLIC_INCLUDE_DIRS "${map_texture_source}"
    PRIVATE_LINKS octaryn::deps::fastgltf octaryn::deps::stb_image octaryn_map_texture_codec)
set_target_properties(octaryn_map_texture_cook PROPERTIES EXCLUDE_FROM_ALL TRUE)
add_custom_target(octaryn_validate_map_texture_cook
    COMMAND "$<TARGET_FILE:octaryn_map_texture_cook>" --self-test
        "${OCTARYN_BUILD_PRESET_ROOT}/tools/map-texture-cache-test"
    DEPENDS octaryn_map_texture_cook VERBATIM)
