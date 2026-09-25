include_guard(GLOBAL)

# Preserve the exact font rasterizer formerly fetched through SDL_ttf 3.2.2.
octaryn_fetch_source_dependency(
    freetype
    GITHUB_REPOSITORY libsdl-org/freetype
    GIT_TAG 9973564cfa63763a3e4ac67c09147899539b1e07
    NO_GIT_SUBMODULES
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "FT_DISABLE_ZLIB ON"
        "FT_DISABLE_BZIP2 ON"
        "FT_DISABLE_PNG ON"
        "FT_DISABLE_BROTLI ON"
        "FT_DISABLE_HARFBUZZ ON"
        "FT_REQUIRE_HARFBUZZ OFF"
        "SKIP_INSTALL_ALL ON")
if(NOT TARGET freetype)
    message(FATAL_ERROR "RmlUi requires the pinned FreeType font rasterizer.")
endif()
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()
set_target_properties(freetype PROPERTIES POSITION_INDEPENDENT_CODE ON)
