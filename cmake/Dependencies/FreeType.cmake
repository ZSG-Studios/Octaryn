include_guard(GLOBAL)

# Preserve the exact font rasterizer formerly fetched through SDL_ttf 3.2.2.
octaryn_fetch_source_dependency(
    freetype
    GITHUB_REPOSITORY ${OCTARYN_DEP_freetype_REPOSITORY}
    GIT_TAG ${OCTARYN_DEP_freetype_TAG}
    NO_GIT_SUBMODULES
    OPTIONS ${OCTARYN_DEP_freetype_OPTIONS})
if(NOT TARGET freetype)
    message(FATAL_ERROR "RmlUi requires the pinned FreeType font rasterizer.")
endif()
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()
set_target_properties(freetype PROPERTIES POSITION_INDEPENDENT_CODE ON)
