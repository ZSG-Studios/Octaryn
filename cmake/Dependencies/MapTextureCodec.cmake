include_guard(GLOBAL)
include(Dependencies/SourceDependencyCache)

# Offline encoder/verification decoder only; never link these into the client.
octaryn_fetch_header_dependency(bc7enc_rdo bc7enc_source_dir
    GITHUB_REPOSITORY richgel999/bc7enc_rdo
    GIT_TAG b9438627eef73a1157e84201b6fa6eb2ffd6d9f0)
if(NOT bc7enc_source_dir)
    message(FATAL_ERROR "The offline map texture cooker requires pinned bc7enc_rdo sources.")
endif()
add_library(octaryn_map_texture_codec STATIC EXCLUDE_FROM_ALL
    "${bc7enc_source_dir}/bc7enc.cpp" "${bc7enc_source_dir}/bc7decomp.cpp")
target_include_directories(octaryn_map_texture_codec SYSTEM PUBLIC "${bc7enc_source_dir}")
target_compile_features(octaryn_map_texture_codec PRIVATE cxx_std_20)
set_target_properties(octaryn_map_texture_codec PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${OCTARYN_SOURCE_DEPENDENCY_BUILD_ROOT}/bc7enc_rdo")
