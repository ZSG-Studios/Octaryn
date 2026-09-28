include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# THE dependency registry. Every external source this workspace fetches is
# pinned in exactly one place: here. To update a dependency, change its
# REPOSITORY/TAG/URL/OPTIONS entry below; no other file carries version
# strings. Consumers in cmake/Dependencies/*.cmake reference only the
# OCTARYN_DEP_<name>_* variables.
#
# TYPE notes:
#   source      -> octaryn_fetch_source_dependency (CMake project, built)
#   header      -> octaryn_fetch_header_dependency (header-only tree)
#   prebuilt    -> externally acquired pin (script/staging), version only
# ---------------------------------------------------------------------------

function(octaryn_register_dependency name)
    cmake_parse_arguments(DEP "HEADER_ONLY;NO_GIT_SUBMODULES" "REPOSITORY;TAG;URL;URL_HASH;SOURCE_SUBDIR" "OPTIONS" ${ARGN})
    set(OCTARYN_DEP_${name}_REPOSITORY "${DEP_REPOSITORY}" PARENT_SCOPE)
    set(OCTARYN_DEP_${name}_TAG "${DEP_TAG}" PARENT_SCOPE)
    set(OCTARYN_DEP_${name}_URL "${DEP_URL}" PARENT_SCOPE)
    set(OCTARYN_DEP_${name}_URL_HASH "${DEP_URL_HASH}" PARENT_SCOPE)
    set(OCTARYN_DEP_${name}_SOURCE_SUBDIR "${DEP_SOURCE_SUBDIR}" PARENT_SCOPE)
    set(OCTARYN_DEP_${name}_OPTIONS "${DEP_OPTIONS}" PARENT_SCOPE)
    if(DEP_HEADER_ONLY)
        set(OCTARYN_DEP_${name}_HEADER_ONLY ON PARENT_SCOPE)
    endif()
    if(DEP_NO_GIT_SUBMODULES)
        set(OCTARYN_DEP_${name}_NO_GIT_SUBMODULES ON PARENT_SCOPE)
    endif()
endfunction()

# --- Native / repo-wide ----------------------------------------------------
octaryn_register_dependency(gpu_perf_api
    TAG 4.4.0.5 SOURCE_SUBDIR 4_4
    URL https://github.com/GPUOpen-Tools/gpu_performance_api/releases/download/v4.4-tag/GPUPerfAPI-4.4.0.5.zip
    URL_HASH SHA256=ca76d7bbba3efa3d3770304653eb92b769b15975e41ee0852c1f82f3a9c29cd3)

octaryn_register_dependency(radeon_developer_tools
    TAG 2026-05-28-1806
    URL https://gpuopen.com/download/RadeonDeveloperToolSuite-2026-05-28-1806.zip
    URL_HASH SHA256=c8ec76abfba6646d0a388f169ceb608f142915997986569ce0e449711ae0720d)

octaryn_register_dependency(glaze
    REPOSITORY stephenberry/glaze TAG v7.4.0
    OPTIONS "glaze_BUILD_TESTS OFF")

octaryn_register_dependency(spdlog
    REPOSITORY gabime/spdlog TAG v1.17.0
    OPTIONS "SPDLOG_BUILD_SHARED OFF" "SPDLOG_BUILD_TESTS OFF" "SPDLOG_BUILD_EXAMPLE OFF"
            "SPDLOG_BUILD_BENCH OFF" "SPDLOG_FMT_EXTERNAL OFF" "SPDLOG_USE_STD_FORMAT OFF")

octaryn_register_dependency(cpptrace
    REPOSITORY jeremy-rifkin/cpptrace TAG v1.0.4)

octaryn_register_dependency(mimalloc
    REPOSITORY microsoft/mimalloc TAG v3.3.1
    OPTIONS "MI_BUILD_TESTS OFF" "MI_BUILD_SHARED OFF")

octaryn_register_dependency(tracy
    REPOSITORY wolfpld/tracy TAG v0.13.1
    OPTIONS "TRACY_ENABLE ON" "TRACY_ON_DEMAND ON" "TRACY_NO_CALLSTACK ON"
            "TRACY_NO_SAMPLING ON" "TRACY_NO_SYSTEM_TRACING ON" "TRACY_NO_FRAME_IMAGE ON")

octaryn_register_dependency(taskflow
    REPOSITORY taskflow/taskflow TAG v4.0.0
    HEADER_ONLY)

octaryn_register_dependency(eigen
    REPOSITORY https://gitlab.com/libeigen/eigen.git TAG 5.0.0
    OPTIONS "BUILD_TESTING OFF" "EIGEN_BUILD_DOC OFF" "EIGEN_BUILD_PKGCONFIG OFF")

octaryn_register_dependency(unordered_dense
    REPOSITORY martinus/unordered_dense TAG v4.8.1
    HEADER_ONLY)

octaryn_register_dependency(zlib
    REPOSITORY madler/zlib TAG v1.3.2
    OPTIONS "ZLIB_BUILD_TESTING OFF" "ZLIB_BUILD_EXAMPLES OFF")

octaryn_register_dependency(lz4
    REPOSITORY lz4/lz4 TAG v1.10.0
    SOURCE_SUBDIR build/cmake)

octaryn_register_dependency(zstd
    REPOSITORY facebook/zstd TAG v1.5.7
    SOURCE_SUBDIR build/cmake
    OPTIONS "BUILD_SHARED_LIBS OFF" "ZSTD_BUILD_PROGRAMS OFF" "ZSTD_BUILD_TESTS OFF")

octaryn_register_dependency(box3d
    REPOSITORY erincatto/box3d TAG v0.1.0
    OPTIONS "BOX3D_BUILD_SAMPLES OFF" "BOX3D_BUILD_TESTS OFF" "BUILD_SHARED_LIBS OFF" "CMAKE_POSITION_INDEPENDENT_CODE ON")

# --- Client ----------------------------------------------------------------
octaryn_register_dependency(bc7enc_rdo
    REPOSITORY richgel999/bc7enc_rdo TAG b9438627eef73a1157e84201b6fa6eb2ffd6d9f0
    HEADER_ONLY)
octaryn_register_dependency(sdl3
    REPOSITORY libsdl-org/SDL TAG release-3.4.4
    OPTIONS "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_GPU OFF" "SDL_RENDER OFF" "SDL_RENDER_GPU OFF"
            "SDL_OPENGL OFF" "SDL_OPENGLES OFF" "SDL_KMSDRM OFF" "SDL_TEST_LIBRARY OFF" "SDL_TESTS OFF")

octaryn_register_dependency(openal
    REPOSITORY kcat/openal-soft TAG 1.25.1
    OPTIONS "ALSOFT_UTILS OFF" "ALSOFT_EXAMPLES OFF" "ALSOFT_TESTS OFF" "LIBTYPE STATIC")

octaryn_register_dependency(miniaudio
    REPOSITORY mackron/miniaudio TAG 0.11.25
    HEADER_ONLY)

octaryn_register_dependency(rmlui
    URL https://github.com/mikke89/RmlUi/archive/refs/tags/6.2.tar.gz
    URL_HASH SHA256=814c3ff7b9666280338d8f0dda85979f5daf028d01c85fc8975431d1e2fd8e8b
    OPTIONS "BUILD_SHARED_LIBS OFF" "RMLUI_FONT_ENGINE freetype" "RMLUI_SAMPLES OFF"
            "RMLUI_TESTS OFF" "RMLUI_LUA_BINDINGS OFF" "RMLUI_LOTTIE_PLUGIN OFF"
            "RMLUI_SVG_PLUGIN OFF" "RMLUI_COMPILER_OPTIONS OFF")

octaryn_register_dependency(freetype
    REPOSITORY libsdl-org/freetype TAG 9973564cfa63763a3e4ac67c09147899539b1e07
    NO_GIT_SUBMODULES
    OPTIONS "BUILD_SHARED_LIBS OFF" "FT_DISABLE_ZLIB ON" "FT_DISABLE_BZIP2 ON" "FT_DISABLE_PNG ON"
            "FT_DISABLE_BROTLI ON" "FT_DISABLE_HARFBUZZ ON" "FT_REQUIRE_HARFBUZZ OFF" "SKIP_INSTALL_ALL ON")

octaryn_register_dependency(meshoptimizer
    REPOSITORY zeux/meshoptimizer TAG 9d9890c73011d75920af614485296d1e03e95448 # v1.2
    OPTIONS "MESHOPT_BUILD_DEMO OFF" "MESHOPT_BUILD_GLTFPACK OFF" "MESHOPT_BUILD_SHARED_LIBS OFF")

octaryn_register_dependency(fastgltf
    REPOSITORY spnda/fastgltf TAG v0.9.0
    OPTIONS "FASTGLTF_DOWNLOAD_SIMDJSON OFF" "FASTGLTF_TESTS OFF")

octaryn_register_dependency(stb
    REPOSITORY nothings/stb TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    HEADER_ONLY)

# --- Prebuilt pins (acquired by scripts, not FetchContent) ------------------
set(OCTARYN_DEP_slang_sdk_version "2026.17.1")
set(OCTARYN_DEP_slang_rhi_commit "e17f6d75f858f9b7cb91bc102a7b8c6fda0435dc")
set(OCTARYN_DEP_fsr2_pin "fsr2-2.2.1-godot-2f698aa5")
