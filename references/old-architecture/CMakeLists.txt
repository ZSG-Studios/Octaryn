cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(
  OctarynEngine
  VERSION 0.1.0
  DESCRIPTION "Octaryn engine"
  LANGUAGES C CXX)

# This codebase does not use C++20 modules; disabling module scanning avoids
# Ninja dyndep instability on large target refreshes.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# This workspace hits intermittent dependency-side regenerate failures; prefer
# explicit reconfigure steps between tranches over Ninja auto-regeneration.
set(CMAKE_SUPPRESS_REGENERATION ON)

include(GNUInstallDirs)

set(OCTARYN_ENGINE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(OCTARYN_WORKSPACE_ROOT_DIR "${OCTARYN_ENGINE_ROOT_DIR}/.." ABSOLUTE)
set(OCTARYN_WORKSPACE_ROOT_DIR "${OCTARYN_WORKSPACE_ROOT_DIR}" CACHE PATH "Workspace root" FORCE)
list(APPEND CMAKE_MODULE_PATH "${OCTARYN_ENGINE_ROOT_DIR}/cmake")
set(OCTARYN_ENGINE_INCLUDE_DIR "${OCTARYN_ENGINE_ROOT_DIR}/include")
set(OCTARYN_ENGINE_ASSET_ROOT_DIR "${OCTARYN_ENGINE_ROOT_DIR}/assets")
include(ProjectOptions)
octaryn_setup_project_options()

include(BuildLayout)

# Presets own the active dependency graph for the engine workspace.

set(BUILD_TESTING OFF CACHE BOOL "Disable dependency test targets" FORCE)

set(octaryn_saved_warn_deprecated ${CMAKE_WARN_DEPRECATED})
set(octaryn_saved_error_deprecated ${CMAKE_ERROR_DEPRECATED})
set(CMAKE_WARN_DEPRECATED OFF)
set(CMAKE_ERROR_DEPRECATED OFF)

include(Dependencies)
octaryn_configure_dependencies()

set(CMAKE_WARN_DEPRECATED ${octaryn_saved_warn_deprecated})
set(CMAKE_ERROR_DEPRECATED ${octaryn_saved_error_deprecated})

get_property(octaryn_buildable_dependency_targets GLOBAL PROPERTY OCTARYN_BUILDABLE_DEPENDENCY_TARGETS)
if(octaryn_buildable_dependency_targets)
  list(REMOVE_DUPLICATES octaryn_buildable_dependency_targets)
  add_custom_target(octaryn_engine_debug_shared_libs DEPENDS ${octaryn_buildable_dependency_targets})
else()
  add_custom_target(octaryn_engine_debug_shared_libs)
endif()

octaryn_product_preset_root(OCTARYN_ENGINE_PRODUCT_PRESET_ROOT "octaryn-engine")

add_subdirectory(source)

if(TARGET octaryn_engine_debug_shared_libs)
  foreach(octaryn_support_target
      imgui
      implot
      implot3d
      imgui_node_editor
      imguizmo
      imanim
      imfiledialog
      octaryn_engine_imgui_backend
      octaryn_engine_shader_assets)
    if(TARGET ${octaryn_support_target})
      add_dependencies(octaryn_engine_debug_shared_libs ${octaryn_support_target})
    endif()
  endforeach()
endif()

if(EXISTS "${OCTARYN_ENGINE_INCLUDE_DIR}")
  install(DIRECTORY "${OCTARYN_ENGINE_INCLUDE_DIR}/"
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif()

if(TARGET octaryn_engine_runtime)
  add_custom_target(octaryn_engine_product DEPENDS octaryn_engine_runtime)
  if(TARGET octaryn_engine_asset_tool)
    add_dependencies(octaryn_engine_product octaryn_engine_asset_tool)
  endif()
  if(TARGET octaryn_engine_shader_tool)
    add_dependencies(octaryn_engine_product octaryn_engine_shader_tool)
  endif()
endif()
