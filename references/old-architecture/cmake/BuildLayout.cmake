include_guard(GLOBAL)

include(CMakeParseArguments)

if(NOT DEFINED OCTARYN_WORKSPACE_ROOT_DIR)
  get_filename_component(OCTARYN_WORKSPACE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
endif()

if(NOT DEFINED OCTARYN_BUILD_PRESET_NAME)
  set(OCTARYN_BUILD_PRESET_NAME "local")
endif()

if(NOT DEFINED OCTARYN_DEPENDENCY_BUCKET_NAME)
  set(OCTARYN_DEPENDENCY_BUCKET_NAME "${OCTARYN_BUILD_PRESET_NAME}")
endif()

set(OCTARYN_REPO_BUILD_ROOT "${OCTARYN_WORKSPACE_ROOT_DIR}/build")
set(OCTARYN_LOG_ROOT "${OCTARYN_WORKSPACE_ROOT_DIR}/logs")
set(OCTARYN_SHARED_BUILD_ROOT "${OCTARYN_REPO_BUILD_ROOT}/shared")
set(OCTARYN_OLD_ARCHITECTURE_DEPS_ROOT "${OCTARYN_REPO_BUILD_ROOT}/dependencies/old-architecture")
set(OCTARYN_SHARED_DEPS_ROOT "${OCTARYN_OLD_ARCHITECTURE_DEPS_ROOT}/${OCTARYN_DEPENDENCY_BUCKET_NAME}")
set(OCTARYN_SHARED_DEPS_LOG_DIR "${OCTARYN_SHARED_DEPS_ROOT}/logs")
set(OCTARYN_SHARED_DEPS_EXPORT_DIR "${OCTARYN_SHARED_DEPS_ROOT}/exports")
set(OCTARYN_SHARED_DEPS_INSTALL_DIR "${OCTARYN_SHARED_DEPS_ROOT}/install")
get_filename_component(OCTARYN_WORKSPACE_CMAKE_STATE_DIR "${CMAKE_BINARY_DIR}" ABSOLUTE)
set(OCTARYN_CPM_DEPS_DIR "${OCTARYN_SHARED_DEPS_ROOT}/_deps")
set(OCTARYN_CPM_CACHE_DIR "${OCTARYN_OLD_ARCHITECTURE_DEPS_ROOT}/cpm-cache")
set(CPM_SOURCE_CACHE "${OCTARYN_CPM_CACHE_DIR}")
set(FETCHCONTENT_BASE_DIR "${OCTARYN_SHARED_DEPS_ROOT}" CACHE PATH "Shared FetchContent root" FORCE)

file(MAKE_DIRECTORY
  "${OCTARYN_SHARED_BUILD_ROOT}"
  "${OCTARYN_OLD_ARCHITECTURE_DEPS_ROOT}"
  "${OCTARYN_CPM_CACHE_DIR}"
  "${OCTARYN_SHARED_DEPS_ROOT}"
  "${OCTARYN_LOG_ROOT}"
  "${OCTARYN_SHARED_DEPS_LOG_DIR}"
  "${OCTARYN_SHARED_DEPS_EXPORT_DIR}"
  "${OCTARYN_SHARED_DEPS_INSTALL_DIR}"
  "${OCTARYN_CPM_DEPS_DIR}"
  "${OCTARYN_WORKSPACE_CMAKE_STATE_DIR}")

function(octaryn_product_preset_root output_var product_name)
  set(${output_var} "${OCTARYN_REPO_BUILD_ROOT}/${product_name}/${OCTARYN_BUILD_PRESET_NAME}" PARENT_SCOPE)
endfunction()

function(octaryn_product_shared_root output_var product_name)
  set(${output_var} "${OCTARYN_REPO_BUILD_ROOT}/${product_name}/shared" PARENT_SCOPE)
endfunction()

function(octaryn_product_stage_root output_var product_name)
  set(${output_var} "${OCTARYN_REPO_BUILD_ROOT}/${product_name}/libs/${OCTARYN_BUILD_PRESET_NAME}" PARENT_SCOPE)
endfunction()

function(octaryn_apply_build_layout target_name)
  set(options)
  set(one_value_args PRODUCT)
  cmake_parse_arguments(OCTARYN_LAYOUT "${options}" "${one_value_args}" "" ${ARGN})

  if(NOT OCTARYN_LAYOUT_PRODUCT)
    message(FATAL_ERROR "octaryn_apply_build_layout(${target_name}) requires PRODUCT <name>")
  endif()

  octaryn_product_preset_root(product_preset_root "${OCTARYN_LAYOUT_PRODUCT}")
  octaryn_product_shared_root(product_shared_root "${OCTARYN_LAYOUT_PRODUCT}")
  octaryn_product_stage_root(product_stage_root "${OCTARYN_LAYOUT_PRODUCT}")

  set(product_log_dir "${OCTARYN_LOG_ROOT}/${OCTARYN_LAYOUT_PRODUCT}/${OCTARYN_BUILD_PRESET_NAME}")
  set(product_generated_dir "${product_preset_root}/generated")
  set(product_runtime_dir "${product_preset_root}/runtime")
  set(product_world_dir "${product_runtime_dir}/world")
  set(product_ui_dir "${product_runtime_dir}/ui")
  set(product_bin_dir "${product_preset_root}/bin")
  set(product_lib_dir "${product_preset_root}/lib")

  file(MAKE_DIRECTORY
    "${product_shared_root}"
    "${product_shared_root}/exports"
    "${product_shared_root}/package"
    "${product_shared_root}/install"
    "${product_stage_root}"
    "${product_log_dir}"
    "${product_generated_dir}"
    "${product_runtime_dir}"
    "${product_world_dir}"
    "${product_ui_dir}"
    "${product_bin_dir}"
    "${product_lib_dir}")

  set(CMAKE_INSTALL_PREFIX "${product_shared_root}/install" CACHE PATH "Product install prefix" FORCE)

  get_target_property(target_type ${target_name} TYPE)
  set(runtime_output_dir "${product_bin_dir}")
  if(NOT target_type STREQUAL "EXECUTABLE")
    set(runtime_output_dir "${product_lib_dir}")
  endif()

  set_target_properties(${target_name} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${product_lib_dir}"
    LIBRARY_OUTPUT_DIRECTORY "${product_lib_dir}"
    RUNTIME_OUTPUT_DIRECTORY "${runtime_output_dir}")

  file(TO_CMAKE_PATH "${product_runtime_dir}" product_runtime_dir_cmake)
  file(TO_CMAKE_PATH "${product_world_dir}" product_world_dir_cmake)
  file(TO_CMAKE_PATH "${product_ui_dir}" product_ui_dir_cmake)

  target_compile_definitions(${target_name}
    PRIVATE
      OCTARYN_RUNTIME_DATA_DIR="${product_runtime_dir_cmake}"
      OCTARYN_WORLD_SAVE_DIR="${product_world_dir_cmake}"
      OCTARYN_UI_STATE_DIR="${product_ui_dir_cmake}")
endfunction()
