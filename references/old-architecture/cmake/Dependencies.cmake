include_guard(GLOBAL)

include(CPM)
include(CMakeParseArguments)
include(FetchContent)

if(OCTARYN_PREFER_SHARED_DEPS)
  set(OCTARYN_DEPENDENCY_SHARED_OPTION ON)
  set(OCTARYN_DEPENDENCY_STATIC_OPTION OFF)
  set(OCTARYN_SPIRV_TOOLS_LIBRARY_TYPE SHARED)
  set(OCTARYN_OPENAL_LIBTYPE SHARED)
else()
  set(OCTARYN_DEPENDENCY_SHARED_OPTION OFF)
  set(OCTARYN_DEPENDENCY_STATIC_OPTION ON)
  set(OCTARYN_SPIRV_TOOLS_LIBRARY_TYPE STATIC)
  set(OCTARYN_OPENAL_LIBTYPE STATIC)
endif()

function(octaryn_make_wrapper wrapper_name)
  if(NOT TARGET ${wrapper_name})
    add_library(${wrapper_name} INTERFACE)
  endif()
endfunction()

function(octaryn_register_buildable_dependency_target target_name)
  if(NOT TARGET ${target_name})
    return()
  endif()

  set(resolved_target ${target_name})
  get_target_property(aliased_target ${target_name} ALIASED_TARGET)
  if(aliased_target)
    set(resolved_target ${aliased_target})
  endif()

  get_target_property(target_imported ${resolved_target} IMPORTED)
  if(target_imported)
    return()
  endif()

  get_target_property(target_type ${resolved_target} TYPE)
  if(NOT target_type STREQUAL "STATIC_LIBRARY"
     AND NOT target_type STREQUAL "SHARED_LIBRARY"
     AND NOT target_type STREQUAL "MODULE_LIBRARY")
    return()
  endif()

  get_property(buildable_targets GLOBAL PROPERTY OCTARYN_BUILDABLE_DEPENDENCY_TARGETS)
  list(APPEND buildable_targets ${resolved_target})
  list(REMOVE_DUPLICATES buildable_targets)
  set_property(GLOBAL PROPERTY OCTARYN_BUILDABLE_DEPENDENCY_TARGETS "${buildable_targets}")
endfunction()

function(octaryn_mark_target_system target_name)
  if(NOT TARGET ${target_name})
    return()
  endif()

  set(resolved_target ${target_name})
  get_target_property(aliased_target ${target_name} ALIASED_TARGET)
  if(aliased_target)
    set(resolved_target ${aliased_target})
  endif()

  if(TARGET ${resolved_target})
    set_property(TARGET ${resolved_target} PROPERTY SYSTEM ON)
  endif()
endfunction()

function(octaryn_silence_external_warning target_name warning_flag)
  if(NOT TARGET ${target_name})
    return()
  endif()

  set(resolved_target ${target_name})
  get_target_property(aliased_target ${target_name} ALIASED_TARGET)
  if(aliased_target)
    set(resolved_target ${aliased_target})
  endif()

  if(TARGET ${resolved_target} AND NOT MSVC)
    target_compile_options(${resolved_target} PRIVATE ${warning_flag})
  endif()
endfunction()

function(octaryn_try_link_wrapper wrapper_name)
  set(options)
  set(oneValueArgs TARGET_NAME)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})

  octaryn_make_wrapper(${wrapper_name})
  if(TARGET ${ARG_TARGET_NAME})
    set_property(TARGET ${wrapper_name} PROPERTY SYSTEM ON)
    octaryn_mark_target_system(${ARG_TARGET_NAME})
    target_link_libraries(${wrapper_name} INTERFACE ${ARG_TARGET_NAME})
    octaryn_register_buildable_dependency_target(${ARG_TARGET_NAME})
  else()
    message(STATUS "Dependency target '${ARG_TARGET_NAME}' was not created; wrapper '${wrapper_name}' remains empty.")
  endif()
endfunction()

function(octaryn_add_package)
  CPMAddPackage(${ARGN})
endfunction()

function(octaryn_make_header_only_wrapper wrapper_name include_dir)
  octaryn_make_wrapper(${wrapper_name})
  set_property(TARGET ${wrapper_name} PROPERTY SYSTEM ON)
  if(EXISTS "${include_dir}")
    target_include_directories(${wrapper_name} SYSTEM INTERFACE "${include_dir}")
  endif()
endfunction()

function(octaryn_make_system_library_wrapper wrapper_name)
  set(options)
  set(oneValueArgs INCLUDE_DIR LIBRARY)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "" ${ARGN})

  octaryn_make_wrapper(${wrapper_name})
  set_property(TARGET ${wrapper_name} PROPERTY SYSTEM ON)
  if(ARG_INCLUDE_DIR)
    target_include_directories(${wrapper_name} SYSTEM INTERFACE "${ARG_INCLUDE_DIR}")
  endif()
  if(ARG_LIBRARY)
    target_link_libraries(${wrapper_name} INTERFACE "${ARG_LIBRARY}")
  endif()
endfunction()

function(octaryn_prepare_git_source_dir source_dir)
  if(EXISTS "${source_dir}")
    if(NOT EXISTS "${source_dir}/.git")
      file(REMOVE_RECURSE "${source_dir}")
    else()
      execute_process(
        COMMAND git -C "${source_dir}" rev-parse --is-inside-work-tree
        RESULT_VARIABLE octaryn_git_repo_result
        OUTPUT_QUIET
        ERROR_QUIET)
      if(NOT octaryn_git_repo_result EQUAL 0)
        file(REMOVE_RECURSE "${source_dir}")
      endif()
    endif()
  endif()
endfunction()

function(octaryn_sync_source_tree source_dir dest_dir stamp_value)
  set(stamp_path "${dest_dir}/.octaryn-sync-stamp")
  set(current_stamp "")
  if(EXISTS "${stamp_path}")
    file(READ "${stamp_path}" current_stamp)
    string(STRIP "${current_stamp}" current_stamp)
  endif()

  if(NOT EXISTS "${dest_dir}" OR NOT current_stamp STREQUAL "${stamp_value}")
    file(REMOVE_RECURSE "${dest_dir}")
    file(MAKE_DIRECTORY "${dest_dir}")
    file(COPY "${source_dir}/" DESTINATION "${dest_dir}")
    file(WRITE "${stamp_path}" "${stamp_value}\n")
  endif()
endfunction()

function(octaryn_replace_in_file file_path search_text replacement_text)
  if(NOT EXISTS "${file_path}")
    return()
  endif()

  file(READ "${file_path}" octaryn_file_contents)
  string(REPLACE "${search_text}" "${replacement_text}" octaryn_updated_contents "${octaryn_file_contents}")
  if(NOT octaryn_updated_contents STREQUAL octaryn_file_contents)
    file(WRITE "${file_path}" "${octaryn_updated_contents}")
  endif()
endfunction()

function(octaryn_patch_shaderc_for_glsl_only source_dir)
  set(octaryn_shaderc_root_cmake "${source_dir}/CMakeLists.txt")
  set(octaryn_shaderc_util_cmake "${source_dir}/libshaderc_util/CMakeLists.txt")
  set(octaryn_shaderc_compiler_cc "${source_dir}/libshaderc_util/src/compiler.cc")

  octaryn_replace_in_file(
    "${octaryn_shaderc_root_cmake}"
    "option(SHADERC_SKIP_EXAMPLES \"Skip building examples\" ${SHADERC_SKIP_EXAMPLES})\nif(NOT ${SHADERC_SKIP_EXAMPLES})\n  set(SHADERC_ENABLE_EXAMPLES ON)\nendif()"
    "option(SHADERC_SKIP_EXAMPLES \"Skip building examples\" ${SHADERC_SKIP_EXAMPLES})\nif(NOT ${SHADERC_SKIP_EXAMPLES})\n  set(SHADERC_ENABLE_EXAMPLES ON)\nendif()\n\noption(SHADERC_SKIP_GLSLC \"Skip building glslc\" ${SHADERC_SKIP_GLSLC})")
  octaryn_replace_in_file(
    "${octaryn_shaderc_root_cmake}"
    "add_subdirectory(glslc)"
    "if(NOT ${SHADERC_SKIP_GLSLC})\n  add_subdirectory(glslc)\nendif()")
  octaryn_replace_in_file(
    "${octaryn_shaderc_util_cmake}"
    "# We use parts of Glslang's HLSL compilation interface, which\n# now requires this preprocessor definition.\nadd_definitions(-DENABLE_HLSL)"
    "if(ENABLE_HLSL)\n  # Shaderc only needs the HLSL preprocessor path when HLSL support stays enabled.\n  add_definitions(-DENABLE_HLSL)\nendif()")
  octaryn_replace_in_file(
    "${octaryn_shaderc_compiler_cc}"
    "  shader.setShiftUavBinding(\n      bases[static_cast<int>(UniformKind::UnorderedAccessView)]);\n  shader.setHlslIoMapping(hlsl_iomap_);\n  shader.setResourceSetBinding(\n"
    "  shader.setShiftUavBinding(\n      bases[static_cast<int>(UniformKind::UnorderedAccessView)]);\n#ifdef ENABLE_HLSL\n  shader.setHlslIoMapping(hlsl_iomap_);\n#endif\n  shader.setResourceSetBinding(\n")
  octaryn_replace_in_file(
    "${octaryn_shaderc_compiler_cc}"
    "  shader.setEnvTarget(target_client_info.target_language,\n                      target_client_info.target_language_version);\n  if (hlsl_functionality1_enabled_) {\n    shader.setEnvTargetHlslFunctionality1();\n  }\n"
    "  shader.setEnvTarget(target_client_info.target_language,\n                      target_client_info.target_language_version);\n#ifdef ENABLE_HLSL\n  if (hlsl_functionality1_enabled_) {\n    shader.setEnvTargetHlslFunctionality1();\n  }\n#endif\n")
  octaryn_replace_in_file(
    "${octaryn_shaderc_compiler_cc}"
    "  shader.setEnvClient(target_client_info.client,\n                      target_client_info.client_version);\n  if (hlsl_functionality1_enabled_) {\n    shader.setEnvTargetHlslFunctionality1();\n  }\n  shader.setInvertY(invert_y_enabled_);\n"
    "  shader.setEnvClient(target_client_info.client,\n                      target_client_info.client_version);\n#ifdef ENABLE_HLSL\n  if (hlsl_functionality1_enabled_) {\n    shader.setEnvTargetHlslFunctionality1();\n  }\n#endif\n  shader.setInvertY(invert_y_enabled_);\n")
  octaryn_replace_in_file(
    "${octaryn_shaderc_compiler_cc}"
    "#ifdef ENABLE_HLSL\n#ifdef ENABLE_HLSL\n  if (hlsl_functionality1_enabled_) {\n    shader.setEnvTargetHlslFunctionality1();\n  }\n#endif\n#endif"
    "#ifdef ENABLE_HLSL\n  if (hlsl_functionality1_enabled_) {\n    shader.setEnvTargetHlslFunctionality1();\n  }\n#endif")
endfunction()

function(octaryn_dependency_alias alias_name wrapper_name)
  if(TARGET ${wrapper_name} AND NOT TARGET ${alias_name})
    add_library(${alias_name} ALIAS ${wrapper_name})
  endif()
endfunction()

function(octaryn_register_dependency_aliases)
  octaryn_dependency_alias(octaryn::deps::spdlog octaryn_deps_spdlog)
  octaryn_dependency_alias(octaryn::deps::eigen octaryn_deps_eigen)
  octaryn_dependency_alias(octaryn::deps::sdl3 octaryn_deps_sdl3)
  octaryn_dependency_alias(octaryn::deps::glaze octaryn_deps_glaze)
  octaryn_dependency_alias(octaryn::deps::unordered_dense octaryn_deps_unordered_dense)
  octaryn_dependency_alias(octaryn::deps::mimalloc octaryn_deps_mimalloc)
  octaryn_dependency_alias(octaryn::deps::imgui octaryn_deps_imgui)
  octaryn_dependency_alias(octaryn::deps::implot octaryn_deps_implot)
  octaryn_dependency_alias(octaryn::deps::implot3d octaryn_deps_implot3d)
  octaryn_dependency_alias(octaryn::deps::imgui_node_editor octaryn_deps_imgui_node_editor)
  octaryn_dependency_alias(octaryn::deps::imguizmo octaryn_deps_imguizmo)
  octaryn_dependency_alias(octaryn::deps::imanim octaryn_deps_imanim)
  octaryn_dependency_alias(octaryn::deps::imfiledialog octaryn_deps_imfiledialog)
  octaryn_dependency_alias(octaryn::deps::zlib octaryn_deps_zlib)
  octaryn_dependency_alias(octaryn::deps::shaderc octaryn_deps_shaderc)
  octaryn_dependency_alias(octaryn::deps::shadercross octaryn_deps_shadercross)
  octaryn_dependency_alias(octaryn::deps::spirv_tools octaryn_deps_spirv_tools)
  octaryn_dependency_alias(octaryn::deps::spirv_cross octaryn_deps_spirv_cross)
  octaryn_dependency_alias(octaryn::deps::sdl3_image octaryn_deps_sdl3_image)
  octaryn_dependency_alias(octaryn::deps::sdl3_ttf octaryn_deps_sdl3_ttf)
  octaryn_dependency_alias(octaryn::deps::fastgltf octaryn_deps_fastgltf)
  octaryn_dependency_alias(octaryn::deps::ktx octaryn_deps_ktx)
  octaryn_dependency_alias(octaryn::deps::meshoptimizer octaryn_deps_meshoptimizer)
  octaryn_dependency_alias(octaryn::deps::ozz_animation octaryn_deps_ozz_animation)
  octaryn_dependency_alias(octaryn::deps::lz4 octaryn_deps_lz4)
  octaryn_dependency_alias(octaryn::deps::zstd octaryn_deps_zstd)
  octaryn_dependency_alias(octaryn::deps::jolt octaryn_deps_jolt)
  octaryn_dependency_alias(octaryn::deps::fastnoise2 octaryn_deps_fastnoise2)
  octaryn_dependency_alias(octaryn::deps::openal octaryn_deps_openal)
  octaryn_dependency_alias(octaryn::deps::miniaudio octaryn_deps_miniaudio)
  octaryn_dependency_alias(octaryn::deps::tracy octaryn_deps_tracy)
  octaryn_dependency_alias(octaryn::deps::cpptrace octaryn_deps_cpptrace)
  octaryn_dependency_alias(octaryn::deps::taskflow octaryn_deps_taskflow)
endfunction()

function(octaryn_ensure_namespaced_interface interface_name)
  if(NOT TARGET ${interface_name})
    add_library(${interface_name} INTERFACE IMPORTED GLOBAL)
  endif()
endfunction()

function(octaryn_add_zlib_dependency)
  octaryn_add_package(
    NAME zlib
    GITHUB_REPOSITORY madler/zlib
    GIT_TAG v1.3.2
    PATCH_COMMAND ""
    OPTIONS
      "BUILD_SHARED_LIBS ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
      "CMAKE_POSITION_INDEPENDENT_CODE ON"
      "ZLIB_BUILD_EXAMPLES OFF")

  if(TARGET ZLIB::ZLIB)
    octaryn_try_link_wrapper(octaryn_deps_zlib TARGET_NAME ZLIB::ZLIB)
    if(TARGET zlibstatic)
      octaryn_register_buildable_dependency_target(zlibstatic)
    elseif(TARGET zlib)
      octaryn_register_buildable_dependency_target(zlib)
    endif()
  elseif(TARGET zlibstatic)
    octaryn_ensure_namespaced_interface(ZLIB::ZLIB)
    set_property(TARGET ZLIB::ZLIB PROPERTY INTERFACE_LINK_LIBRARIES zlibstatic)
    if(NOT TARGET ZLIB::ZLIB)
      message(FATAL_ERROR "Failed to create ZLIB::ZLIB compatibility target")
    endif()
    octaryn_try_link_wrapper(octaryn_deps_zlib TARGET_NAME ZLIB::ZLIB)
    octaryn_register_buildable_dependency_target(zlibstatic)
  elseif(TARGET zlib)
    octaryn_ensure_namespaced_interface(ZLIB::ZLIB)
    set_property(TARGET ZLIB::ZLIB PROPERTY INTERFACE_LINK_LIBRARIES zlib)
    octaryn_try_link_wrapper(octaryn_deps_zlib TARGET_NAME ZLIB::ZLIB)
    octaryn_register_buildable_dependency_target(zlib)
  else()
    octaryn_make_wrapper(octaryn_deps_zlib)
  endif()
endfunction()

function(octaryn_add_core_dependencies)
  octaryn_add_package(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.17.0
    OPTIONS
      "SPDLOG_BUILD_SHARED OFF"
      "SPDLOG_BUILD_TESTS OFF"
      "SPDLOG_BUILD_EXAMPLE OFF"
      "SPDLOG_BUILD_BENCH OFF"
      "SPDLOG_FMT_EXTERNAL OFF"
      "SPDLOG_USE_STD_FORMAT OFF")
  if(TARGET spdlog::spdlog_header_only)
    octaryn_try_link_wrapper(octaryn_deps_spdlog TARGET_NAME spdlog::spdlog_header_only)
  else()
    octaryn_try_link_wrapper(octaryn_deps_spdlog TARGET_NAME spdlog::spdlog)
  endif()

  octaryn_add_package(
    NAME Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 5.0.0
    OPTIONS "BUILD_TESTING OFF" "EIGEN_BUILD_DOC OFF" "EIGEN_BUILD_PKGCONFIG OFF")
  octaryn_try_link_wrapper(octaryn_deps_eigen TARGET_NAME Eigen3::Eigen)

  octaryn_add_package(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.4
    OPTIONS
      "SDL_SHARED ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
      "SDL_STATIC ${OCTARYN_DEPENDENCY_STATIC_OPTION}"
      "SDL_TEST_LIBRARY OFF"
      "SDL_TESTS OFF")
  octaryn_try_link_wrapper(octaryn_deps_sdl3 TARGET_NAME SDL3::SDL3)

  octaryn_add_package(
    NAME glaze
    GITHUB_REPOSITORY stephenberry/glaze
    VERSION 7.4.0
    OPTIONS "glaze_BUILD_TESTS OFF")
  octaryn_try_link_wrapper(octaryn_deps_glaze TARGET_NAME glaze::glaze)

  octaryn_add_package(
    NAME unordered_dense
    GITHUB_REPOSITORY martinus/unordered_dense
    VERSION 4.8.1)
  if(TARGET ankerl::unordered_dense)
    octaryn_try_link_wrapper(octaryn_deps_unordered_dense TARGET_NAME ankerl::unordered_dense)
  elseif(EXISTS "${unordered_dense_SOURCE_DIR}/include")
    octaryn_make_header_only_wrapper(octaryn_deps_unordered_dense "${unordered_dense_SOURCE_DIR}/include")
  else()
    octaryn_make_wrapper(octaryn_deps_unordered_dense)
  endif()

  octaryn_add_package(
    NAME mimalloc
    GITHUB_REPOSITORY microsoft/mimalloc
    VERSION 3.3.1
    OPTIONS "MI_BUILD_TESTS OFF" "MI_BUILD_SHARED ${OCTARYN_DEPENDENCY_SHARED_OPTION}")
  if(OCTARYN_PREFER_SHARED_DEPS AND TARGET mimalloc)
    octaryn_try_link_wrapper(octaryn_deps_mimalloc TARGET_NAME mimalloc)
  elseif(TARGET mimalloc-static)
    octaryn_try_link_wrapper(octaryn_deps_mimalloc TARGET_NAME mimalloc-static)
  elseif(TARGET mimalloc)
    octaryn_try_link_wrapper(octaryn_deps_mimalloc TARGET_NAME mimalloc)
  else()
    octaryn_make_wrapper(octaryn_deps_mimalloc)
  endif()
endfunction()

function(octaryn_add_shadercross_dependency)
  set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "Enable SPIRV-Cross GLSL backend" FORCE)
  set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "Disable SPIRV-Cross HLSL backend" FORCE)
  set(SPIRV_CROSS_ENABLE_MSL ON CACHE BOOL "Enable SPIRV-Cross MSL backend" FORCE)
  set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "Disable SPIRV-Cross C++ backend" FORCE)
  set(SPIRV_CROSS_ENABLE_REFLECT ON CACHE BOOL "Enable SPIRV-Cross reflection backend" FORCE)
  set(SPIRV_CROSS_ENABLE_C_API ON CACHE BOOL "Enable SPIRV-Cross C API backend" FORCE)
  set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "Disable SPIRV-Cross util backend" FORCE)

  octaryn_add_package(
    NAME SDL3_shadercross
    GITHUB_REPOSITORY libsdl-org/SDL_shadercross
    GIT_TAG main
    OPTIONS
      "SDLSHADERCROSS_SHARED ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
      "SDLSHADERCROSS_STATIC ${OCTARYN_DEPENDENCY_STATIC_OPTION}"
      "SDLSHADERCROSS_CLI OFF"
      "SDLSHADERCROSS_INSTALL OFF"
      "SDLSHADERCROSS_TESTS OFF"
      "SDLSHADERCROSS_VENDORED ON"
      "SDLSHADERCROSS_SPIRVCROSS_SHARED OFF"
      "SPIRV_CROSS_ENABLE_GLSL ON"
      "SPIRV_CROSS_ENABLE_HLSL OFF"
      "SPIRV_CROSS_ENABLE_MSL ON"
      "SPIRV_CROSS_ENABLE_CPP OFF"
      "SPIRV_CROSS_ENABLE_REFLECT ON"
      "SPIRV_CROSS_ENABLE_C_API ON"
      "SPIRV_CROSS_ENABLE_UTIL OFF"
      "SPIRV_SKIP_EXECUTABLES ON"
      "SPIRV_SKIP_TESTS ON"
      "SPIRV_TOOLS_BUILD_STATIC ${OCTARYN_DEPENDENCY_STATIC_OPTION}"
      "SPIRV_TOOLS_LIBRARY_TYPE ${OCTARYN_SPIRV_TOOLS_LIBRARY_TYPE}"
      "SDLSHADERCROSS_DXC OFF")

  octaryn_make_wrapper(octaryn_deps_shadercross)
  set_property(TARGET octaryn_deps_shadercross PROPERTY SYSTEM ON)
  if(TARGET SDL3_shadercross::SDL3_shadercross)
    octaryn_mark_target_system(SDL3_shadercross::SDL3_shadercross)
    octaryn_silence_external_warning(SDL3_shadercross::SDL3_shadercross -Wno-variadic-macro-arguments-omitted)
    target_link_libraries(octaryn_deps_shadercross INTERFACE SDL3_shadercross::SDL3_shadercross)
    octaryn_register_buildable_dependency_target(SDL3_shadercross::SDL3_shadercross)
  elseif(TARGET SDL3_shadercross-static)
    octaryn_mark_target_system(SDL3_shadercross-static)
    octaryn_silence_external_warning(SDL3_shadercross-static -Wno-variadic-macro-arguments-omitted)
    target_link_libraries(octaryn_deps_shadercross INTERFACE SDL3_shadercross-static)
  endif()
endfunction()

function(octaryn_add_shaderc_dependency)
  set(octaryn_shaderc_source_dir "${CPM_SOURCE_CACHE}/shaderc/octaryn-shaderc-src")
  set(octaryn_spirv_headers_source_dir "${CPM_SOURCE_CACHE}/spirv-headers/octaryn-spirv-headers-src")
  set(octaryn_glslang_source_dir "${CPM_SOURCE_CACHE}/glslang/octaryn-glslang-src")

  octaryn_prepare_git_source_dir("${octaryn_shaderc_source_dir}")
  octaryn_prepare_git_source_dir("${octaryn_spirv_headers_source_dir}")
  octaryn_prepare_git_source_dir("${octaryn_glslang_source_dir}")

  octaryn_add_package(NAME octaryn_shaderc GIT_REPOSITORY https://github.com/google/shaderc.git GIT_TAG v2026.2 SOURCE_DIR "${octaryn_shaderc_source_dir}" DOWNLOAD_ONLY YES)
  octaryn_add_package(NAME octaryn_shaderc_spirv_headers GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git GIT_TAG vulkan-sdk-1.4.341.0 SOURCE_DIR "${octaryn_spirv_headers_source_dir}" DOWNLOAD_ONLY YES)
  octaryn_add_package(NAME octaryn_shaderc_glslang GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git GIT_TAG vulkan-sdk-1.4.341.0 SOURCE_DIR "${octaryn_glslang_source_dir}" DOWNLOAD_ONLY YES)

  file(MAKE_DIRECTORY "${octaryn_shaderc_source_dir}/third_party")
  octaryn_sync_source_tree("${octaryn_glslang_source_dir}" "${octaryn_shaderc_source_dir}/third_party/glslang" "glslang:vulkan-sdk-1.4.341.0")
  octaryn_patch_shaderc_for_glsl_only("${octaryn_shaderc_source_dir}")

  set(SHADERC_GLSLANG_DIR "${octaryn_shaderc_source_dir}/third_party/glslang" CACHE STRING "Shaderc glslang source" FORCE)
  set(SHADERC_SKIP_TESTS ON CACHE BOOL "Skip shaderc tests" FORCE)
  set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "Skip shaderc examples" FORCE)
  set(SHADERC_SKIP_GLSLC ON CACHE BOOL "Skip building glslc" FORCE)
  set(SHADERC_SKIP_COPYRIGHT_CHECK ON CACHE BOOL "Skip shaderc copyright check" FORCE)
  set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Skip glslang standalone binaries" FORCE)
  set(ENABLE_HLSL OFF CACHE BOOL "Disable glslang HLSL input support" FORCE)
  set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building SPIRV-Tools executables" FORCE)
  set(SPIRV_SKIP_TESTS ON CACHE BOOL "Skip building SPIRV-Tools tests" FORCE)
  set(SPIRV_TOOLS_BUILD_STATIC ${OCTARYN_DEPENDENCY_STATIC_OPTION} CACHE BOOL "Build SPIRV-Tools static library target" FORCE)
  set(SPIRV_TOOLS_LIBRARY_TYPE ${OCTARYN_SPIRV_TOOLS_LIBRARY_TYPE} CACHE STRING "SPIRV-Tools library type" FORCE)
  add_subdirectory("${octaryn_shaderc_source_dir}" "${OCTARYN_CPM_DEPS_DIR}/shaderc-build")

  octaryn_make_wrapper(octaryn_deps_shaderc)
  set_property(TARGET octaryn_deps_shaderc PROPERTY SYSTEM ON)
  if(OCTARYN_PREFER_SHARED_DEPS AND TARGET shaderc_shared)
    octaryn_mark_target_system(shaderc_shared)
    target_link_libraries(octaryn_deps_shaderc INTERFACE shaderc_shared)
    octaryn_register_buildable_dependency_target(shaderc_shared)
  elseif(TARGET shaderc_combined)
    octaryn_mark_target_system(shaderc_combined)
    target_link_libraries(octaryn_deps_shaderc INTERFACE shaderc_combined)
  elseif(TARGET shaderc)
    octaryn_mark_target_system(shaderc)
    target_link_libraries(octaryn_deps_shaderc INTERFACE shaderc)
  elseif(TARGET shaderc_shared)
    octaryn_mark_target_system(shaderc_shared)
    target_link_libraries(octaryn_deps_shaderc INTERFACE shaderc_shared)
    octaryn_register_buildable_dependency_target(shaderc_shared)
  endif()
endfunction()

function(octaryn_add_spirv_tools_dependency)
  if(OCTARYN_PREFER_SHARED_DEPS AND TARGET SPIRV-Tools)
    octaryn_try_link_wrapper(octaryn_deps_spirv_tools TARGET_NAME SPIRV-Tools)
  elseif(TARGET SPIRV-Tools-static)
    octaryn_try_link_wrapper(octaryn_deps_spirv_tools TARGET_NAME SPIRV-Tools-static)
  elseif(TARGET SPIRV-Tools-opt)
    octaryn_try_link_wrapper(octaryn_deps_spirv_tools TARGET_NAME SPIRV-Tools-opt)
  else()
    octaryn_add_package(
      NAME SPIRV-Tools
      GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
      GIT_TAG vulkan-sdk-1.4.341.0
      OPTIONS
        "BUILD_SHARED_LIBS ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
        "SPIRV_SKIP_EXECUTABLES ON"
        "SPIRV_SKIP_TESTS ON"
        "SPIRV_TOOLS_BUILD_STATIC ${OCTARYN_DEPENDENCY_STATIC_OPTION}"
        "SPIRV_TOOLS_LIBRARY_TYPE ${OCTARYN_SPIRV_TOOLS_LIBRARY_TYPE}"
        "SPIRV_WERROR OFF")

    if(OCTARYN_PREFER_SHARED_DEPS AND TARGET SPIRV-Tools)
      octaryn_try_link_wrapper(octaryn_deps_spirv_tools TARGET_NAME SPIRV-Tools)
    elseif(TARGET SPIRV-Tools-static)
      octaryn_try_link_wrapper(octaryn_deps_spirv_tools TARGET_NAME SPIRV-Tools-static)
    elseif(TARGET SPIRV-Tools-opt)
      octaryn_try_link_wrapper(octaryn_deps_spirv_tools TARGET_NAME SPIRV-Tools-opt)
    else()
      octaryn_make_wrapper(octaryn_deps_spirv_tools)
    endif()
  endif()
endfunction()

function(octaryn_add_spirv_cross_dependency)
  if(TARGET spirv-cross-core)
    octaryn_make_wrapper(octaryn_deps_spirv_cross)
    set_property(TARGET octaryn_deps_spirv_cross PROPERTY SYSTEM ON)
    octaryn_mark_target_system(spirv-cross-core)
    octaryn_mark_target_system(spirv-cross-glsl)
    octaryn_mark_target_system(spirv-cross-msl)
    octaryn_mark_target_system(spirv-cross-reflect)
    octaryn_silence_external_warning(spirv-cross-msl -Wno-c++17-attribute-extensions)
    target_link_libraries(
      octaryn_deps_spirv_cross
      INTERFACE
        spirv-cross-core
        spirv-cross-glsl
        spirv-cross-msl
        spirv-cross-reflect)
    octaryn_register_buildable_dependency_target(spirv-cross-core)
    octaryn_register_buildable_dependency_target(spirv-cross-glsl)
    octaryn_register_buildable_dependency_target(spirv-cross-msl)
    octaryn_register_buildable_dependency_target(spirv-cross-reflect)
  else()
    octaryn_add_package(
      NAME SPIRV-Cross
      GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
      GIT_TAG vulkan-sdk-1.4.341.0
      OPTIONS
        "SPIRV_CROSS_SHARED ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
        "SPIRV_CROSS_STATIC ${OCTARYN_DEPENDENCY_STATIC_OPTION}"
        "SPIRV_CROSS_CLI OFF"
        "SPIRV_CROSS_ENABLE_TESTS OFF"
        "SPIRV_CROSS_ENABLE_GLSL ON"
        "SPIRV_CROSS_ENABLE_HLSL OFF"
        "SPIRV_CROSS_ENABLE_MSL ON"
        "SPIRV_CROSS_ENABLE_CPP OFF"
        "SPIRV_CROSS_ENABLE_REFLECT ON"
        "SPIRV_CROSS_ENABLE_UTIL OFF")

    if(TARGET spirv-cross-core)
      octaryn_make_wrapper(octaryn_deps_spirv_cross)
      set_property(TARGET octaryn_deps_spirv_cross PROPERTY SYSTEM ON)
      octaryn_mark_target_system(spirv-cross-core)
      octaryn_mark_target_system(spirv-cross-glsl)
      octaryn_mark_target_system(spirv-cross-msl)
      octaryn_mark_target_system(spirv-cross-reflect)
      octaryn_silence_external_warning(spirv-cross-msl -Wno-c++17-attribute-extensions)
      target_link_libraries(
        octaryn_deps_spirv_cross
        INTERFACE
          spirv-cross-core
          spirv-cross-glsl
          spirv-cross-msl
          spirv-cross-reflect)
      octaryn_register_buildable_dependency_target(spirv-cross-core)
      octaryn_register_buildable_dependency_target(spirv-cross-glsl)
      octaryn_register_buildable_dependency_target(spirv-cross-msl)
      octaryn_register_buildable_dependency_target(spirv-cross-reflect)
    else()
      octaryn_make_wrapper(octaryn_deps_spirv_cross)
    endif()
  endif()
endfunction()

function(octaryn_add_ui_dependencies)
  octaryn_add_package(
    NAME SDL3_ttf
    GITHUB_REPOSITORY libsdl-org/SDL_ttf
    GIT_TAG release-3.2.2
    OPTIONS
      "BUILD_SHARED_LIBS ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
      "SDLTTF_SAMPLES OFF"
      "SDLTTF_TESTS OFF")
  octaryn_try_link_wrapper(octaryn_deps_sdl3_ttf TARGET_NAME SDL3_ttf::SDL3_ttf)
endfunction()

function(octaryn_add_sdl3_image_dependency)
  octaryn_add_package(
    NAME SDL3_image
    GITHUB_REPOSITORY libsdl-org/SDL_image
    GIT_TAG release-3.4.2
    OPTIONS
      "BUILD_SHARED_LIBS ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
      "SDLIMAGE_AVIF OFF"
      "SDLIMAGE_TESTS OFF"
      "SDLIMAGE_BACKEND_IMAGEIO OFF")
  octaryn_try_link_wrapper(octaryn_deps_sdl3_image TARGET_NAME SDL3_image::SDL3_image)
endfunction()

function(octaryn_add_imgui_dependency)
  set(octaryn_imgui_source_dir "${CPM_SOURCE_CACHE}/imgui/octaryn-imgui-src")
  octaryn_add_package(
    NAME imgui
    GITHUB_REPOSITORY pthom/imgui
    GIT_TAG 285b38e2a7cfb2850ef27385f4e70df0f74f6b97
    SOURCE_DIR "${octaryn_imgui_source_dir}"
    DOWNLOAD_ONLY YES)
  set(OCTARYN_IMGUI_SOURCE_DIR "${octaryn_imgui_source_dir}" CACHE INTERNAL "Fetched Dear ImGui source dir" FORCE)

  if(NOT TARGET imgui)
    file(GLOB imgui_sources CONFIGURE_DEPENDS
      "${octaryn_imgui_source_dir}/*.h"
      "${octaryn_imgui_source_dir}/*.cpp"
      "${octaryn_imgui_source_dir}/misc/cpp/*.h"
      "${octaryn_imgui_source_dir}/misc/cpp/*.cpp")
    add_library(imgui STATIC ${imgui_sources})
    target_include_directories(imgui
      PUBLIC
        "$<BUILD_INTERFACE:${octaryn_imgui_source_dir}>"
        "$<BUILD_INTERFACE:${octaryn_imgui_source_dir}/backends>"
        "$<BUILD_INTERFACE:${octaryn_imgui_source_dir}/misc/cpp>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
    set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()

  octaryn_try_link_wrapper(octaryn_deps_imgui TARGET_NAME imgui)
endfunction()

function(octaryn_add_imgui_extension_dependencies)
  set(octaryn_implot_source_dir "${CPM_SOURCE_CACHE}/implot/octaryn-implot-src")
  set(octaryn_implot3d_source_dir "${CPM_SOURCE_CACHE}/implot3d/octaryn-implot3d-src")
  set(octaryn_imgui_node_editor_source_dir "${CPM_SOURCE_CACHE}/imgui_node_editor/octaryn-imgui-node-editor-src")
  set(octaryn_imguizmo_source_dir "${CPM_SOURCE_CACHE}/imguizmo/octaryn-imguizmo-src")
  set(octaryn_imanim_source_dir "${CPM_SOURCE_CACHE}/imanim/octaryn-imanim-src")
  set(octaryn_imfiledialog_source_dir "${CPM_SOURCE_CACHE}/imfiledialog/octaryn-imfiledialog-src")
  set(octaryn_imgui_shim_include_dir "${CMAKE_BINARY_DIR}/generated/imgui_shim")

  file(MAKE_DIRECTORY "${octaryn_imgui_shim_include_dir}/imgui")
  file(WRITE "${octaryn_imgui_shim_include_dir}/imgui/imgui.h" "#pragma once\n#include <imgui.h>\n")
  file(WRITE "${octaryn_imgui_shim_include_dir}/imgui/imgui_internal.h" "#pragma once\n#include <imgui_internal.h>\n")

  octaryn_add_package(
    NAME implot
    GITHUB_REPOSITORY pthom/implot
    GIT_TAG e6c36daf587b5eafebb533af1826b6d114b45421
    SOURCE_DIR "${octaryn_implot_source_dir}"
    DOWNLOAD_ONLY YES)
  if(NOT TARGET implot)
    add_library(implot STATIC
      "${octaryn_implot_source_dir}/implot.cpp"
      "${octaryn_implot_source_dir}/implot_demo.cpp"
      "${octaryn_implot_source_dir}/implot_items.cpp")
    target_include_directories(implot PUBLIC "$<BUILD_INTERFACE:${octaryn_implot_source_dir}>")
    target_link_libraries(implot PUBLIC imgui)
    target_compile_definitions(implot PRIVATE "IMPLOT_CUSTOM_NUMERIC_TYPES=(signed char)(unsigned char)(signed short)(unsigned short)(signed int)(unsigned int)(signed long)(unsigned long)(signed long long)(unsigned long long)(float)(double)(long double)")
    set_target_properties(implot PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  octaryn_try_link_wrapper(octaryn_deps_implot TARGET_NAME implot)

  octaryn_add_package(
    NAME implot3d
    GITHUB_REPOSITORY pthom/implot3d
    GIT_TAG eb4ccd75f34b07646dfefb13b14f2df728bfd7ca
    SOURCE_DIR "${octaryn_implot3d_source_dir}"
    DOWNLOAD_ONLY YES)
  if(NOT TARGET implot3d)
    add_library(implot3d STATIC
      "${octaryn_implot3d_source_dir}/implot3d.cpp"
      "${octaryn_implot3d_source_dir}/implot3d_demo.cpp"
      "${octaryn_implot3d_source_dir}/implot3d_items.cpp"
      "${octaryn_implot3d_source_dir}/implot3d_meshes.cpp")
    target_include_directories(implot3d PUBLIC "$<BUILD_INTERFACE:${octaryn_implot3d_source_dir}>")
    target_link_libraries(implot3d PUBLIC imgui implot)
    target_compile_definitions(implot3d PRIVATE "IMPLOT3D_CUSTOM_NUMERIC_TYPES=(signed char)(unsigned char)(signed short)(unsigned short)(signed int)(unsigned int)(signed long)(unsigned long)(signed long long)(unsigned long long)(float)(double)(long double)")
    set_target_properties(implot3d PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  octaryn_try_link_wrapper(octaryn_deps_implot3d TARGET_NAME implot3d)

  octaryn_add_package(
    NAME imgui_node_editor
    GITHUB_REPOSITORY pthom/imgui-node-editor
    GIT_TAG 432c515535f4755c89235d58e71343c7c62ed317
    SOURCE_DIR "${octaryn_imgui_node_editor_source_dir}"
    DOWNLOAD_ONLY YES)
  if(NOT TARGET imgui_node_editor)
    file(GLOB imgui_node_editor_sources CONFIGURE_DEPENDS
      "${octaryn_imgui_node_editor_source_dir}/*.cpp"
      "${octaryn_imgui_node_editor_source_dir}/*.h")
    add_library(imgui_node_editor STATIC ${imgui_node_editor_sources})
    target_include_directories(imgui_node_editor PUBLIC "$<BUILD_INTERFACE:${octaryn_imgui_node_editor_source_dir}>")
    target_link_libraries(imgui_node_editor PUBLIC imgui)
    set_target_properties(imgui_node_editor PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  octaryn_try_link_wrapper(octaryn_deps_imgui_node_editor TARGET_NAME imgui_node_editor)

  octaryn_add_package(
    NAME imguizmo
    GITHUB_REPOSITORY pthom/ImGuizmo
    GIT_TAG bbf06a1b0a1f18668acc6687ae283d6a12368271
    SOURCE_DIR "${octaryn_imguizmo_source_dir}"
    DOWNLOAD_ONLY YES)
  if(NOT TARGET imguizmo)
    add_library(imguizmo STATIC
      "${octaryn_imguizmo_source_dir}/ImGuizmo.cpp"
      "${octaryn_imguizmo_source_dir}/GraphEditor.cpp"
      "${octaryn_imguizmo_source_dir}/ImCurveEdit.cpp"
      "${octaryn_imguizmo_source_dir}/ImGradient.cpp"
      "${octaryn_imguizmo_source_dir}/ImSequencer.cpp")
    target_include_directories(imguizmo PUBLIC "$<BUILD_INTERFACE:${octaryn_imguizmo_source_dir}>")
    target_link_libraries(imguizmo PUBLIC imgui)
    set_target_properties(imguizmo PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  octaryn_try_link_wrapper(octaryn_deps_imguizmo TARGET_NAME imguizmo)

  octaryn_add_package(
    NAME imanim
    GITHUB_REPOSITORY pthom/ImAnim
    GIT_TAG 51b78e795cf4d64f7d016d148b46a02e837e4023
    SOURCE_DIR "${octaryn_imanim_source_dir}"
    DOWNLOAD_ONLY YES)
  if(NOT TARGET imanim)
    add_library(imanim STATIC
      "${octaryn_imanim_source_dir}/im_anim.cpp"
      "${octaryn_imanim_source_dir}/im_anim_demo_basics.cpp"
      "${octaryn_imanim_source_dir}/im_anim_demo.cpp"
      "${octaryn_imanim_source_dir}/im_anim_doc.cpp"
      "${octaryn_imanim_source_dir}/im_anim_usecase.cpp")
    target_include_directories(imanim PUBLIC "$<BUILD_INTERFACE:${octaryn_imanim_source_dir}>")
    target_link_libraries(imanim PUBLIC imgui)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(imanim PRIVATE -Wno-unused-result -Wno-format-truncation)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(imanim PRIVATE -Wno-unknown-warning-option -Wno-nontrivial-memaccess -Wno-nontrivial-memcall)
    endif()
    set_target_properties(imanim PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  octaryn_try_link_wrapper(octaryn_deps_imanim TARGET_NAME imanim)

  octaryn_add_package(
    NAME imfiledialog
    GITHUB_REPOSITORY pthom/ImFileDialog
    GIT_TAG c9819dd90450262efe7682839bb751c38173e1d8
    SOURCE_DIR "${octaryn_imfiledialog_source_dir}"
    DOWNLOAD_ONLY YES)
  if(NOT TARGET imfiledialog)
    add_library(imfiledialog STATIC
      "${octaryn_imfiledialog_source_dir}/ImFileDialog.cpp")
    target_include_directories(imfiledialog
      PUBLIC
        "$<BUILD_INTERFACE:${octaryn_imfiledialog_source_dir}>"
      PRIVATE
        "$<BUILD_INTERFACE:${octaryn_imgui_shim_include_dir}>"
        "$<BUILD_INTERFACE:${CPM_PACKAGE_SDL3_image_SOURCE_DIR}/include>")
    target_link_libraries(imfiledialog PUBLIC imgui octaryn_deps_sdl3_image)
    set_target_properties(imfiledialog PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
  endif()
  octaryn_try_link_wrapper(octaryn_deps_imfiledialog TARGET_NAME imfiledialog)
endfunction()

function(octaryn_add_asset_dependencies)
  octaryn_add_sdl3_image_dependency()

  octaryn_add_package(
    NAME fastgltf
    GITHUB_REPOSITORY spnda/fastgltf
    VERSION 0.9.0
    OPTIONS "FASTGLTF_DOWNLOAD_SIMDJSON OFF" "FASTGLTF_TESTS OFF")
  octaryn_try_link_wrapper(octaryn_deps_fastgltf TARGET_NAME fastgltf::fastgltf)

  octaryn_add_package(
    NAME KTX-Software
    GITHUB_REPOSITORY KhronosGroup/KTX-Software
    VERSION 4.4.2
    OPTIONS
      "BUILD_SHARED_LIBS ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
      "KTX_FEATURE_TESTS OFF"
      "KTX_FEATURE_TOOLS OFF"
      "KTX_FEATURE_LOADTEST_APPS OFF"
      "KTX_FEATURE_GL_UPLOAD OFF"
      "ASTCENC_INVARIANCE OFF"
      "CMAKE_CXX_FLAGS -Wno-overriding-option -Wno-nontrivial-memcall")
  if(TARGET KTX::ktx)
    octaryn_mark_target_system(KTX::ktx)
    octaryn_try_link_wrapper(octaryn_deps_ktx TARGET_NAME KTX::ktx)
  else()
    octaryn_make_wrapper(octaryn_deps_ktx)
  endif()

  octaryn_add_package(
    NAME meshoptimizer
    GITHUB_REPOSITORY zeux/meshoptimizer
    VERSION 1.1.1)
  octaryn_try_link_wrapper(octaryn_deps_meshoptimizer TARGET_NAME meshoptimizer)
endfunction()

function(octaryn_add_animation_dependencies)
  octaryn_add_package(
    NAME ozz_animation
    GITHUB_REPOSITORY guillaumeblanc/ozz-animation
    GIT_TAG 0.16.0
    OPTIONS
      "BUILD_SHARED_LIBS ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
      "ozz_build_tools OFF"
      "ozz_build_fbx OFF"
      "ozz_build_gltf OFF"
      "ozz_build_data OFF"
      "ozz_build_samples OFF"
      "ozz_build_howtos OFF"
      "ozz_build_tests OFF"
      "ozz_build_postfix OFF")
  octaryn_try_link_wrapper(octaryn_deps_ozz_animation TARGET_NAME ozz_animation)
endfunction()

function(octaryn_add_storage_dependencies)
  octaryn_add_package(
    NAME lz4
    GITHUB_REPOSITORY lz4/lz4
    VERSION 1.10.0
    SOURCE_SUBDIR build/cmake)
  if(OCTARYN_PREFER_SHARED_DEPS AND TARGET lz4::lz4)
    octaryn_try_link_wrapper(octaryn_deps_lz4 TARGET_NAME lz4::lz4)
  elseif(TARGET lz4_static)
    octaryn_try_link_wrapper(octaryn_deps_lz4 TARGET_NAME lz4_static)
  else()
    octaryn_make_wrapper(octaryn_deps_lz4)
  endif()

  octaryn_add_package(
    NAME zstd
    GITHUB_REPOSITORY facebook/zstd
    VERSION 1.5.7
    SOURCE_SUBDIR build/cmake
    OPTIONS
      "BUILD_SHARED_LIBS ${OCTARYN_DEPENDENCY_SHARED_OPTION}"
      "ZSTD_BUILD_PROGRAMS OFF"
      "ZSTD_BUILD_TESTS OFF")
  if(OCTARYN_PREFER_SHARED_DEPS AND TARGET libzstd_shared)
    octaryn_try_link_wrapper(octaryn_deps_zstd TARGET_NAME libzstd_shared)
  elseif(OCTARYN_PREFER_SHARED_DEPS AND TARGET zstd::libzstd_shared)
    octaryn_try_link_wrapper(octaryn_deps_zstd TARGET_NAME zstd::libzstd_shared)
  elseif(TARGET libzstd_static)
    octaryn_try_link_wrapper(octaryn_deps_zstd TARGET_NAME libzstd_static)
  elseif(TARGET zstd::libzstd_static)
    octaryn_try_link_wrapper(octaryn_deps_zstd TARGET_NAME zstd::libzstd_static)
  else()
    octaryn_make_wrapper(octaryn_deps_zstd)
  endif()
endfunction()

function(octaryn_add_taskflow_dependency)
  octaryn_add_package(
    NAME Taskflow
    GITHUB_REPOSITORY taskflow/taskflow
    VERSION 4.0.0
    DOWNLOAD_ONLY YES)
  if(EXISTS "${Taskflow_SOURCE_DIR}/taskflow")
    octaryn_make_header_only_wrapper(octaryn_deps_taskflow "${Taskflow_SOURCE_DIR}")
  elseif(EXISTS "${Taskflow_SOURCE_DIR}")
    octaryn_make_header_only_wrapper(octaryn_deps_taskflow "${Taskflow_SOURCE_DIR}")
  else()
    octaryn_make_wrapper(octaryn_deps_taskflow)
  endif()
endfunction()

function(octaryn_add_worldgen_dependencies)
  octaryn_add_package(
    NAME FastNoise2
    GITHUB_REPOSITORY Auburn/FastNoise2
    VERSION 1.1.1
    OPTIONS "FASTNOISE2_NOISETOOL OFF")
  if(TARGET FastNoise)
    octaryn_try_link_wrapper(octaryn_deps_fastnoise2 TARGET_NAME FastNoise)
  elseif(TARGET FastNoise2)
    octaryn_try_link_wrapper(octaryn_deps_fastnoise2 TARGET_NAME FastNoise2)
  else()
    octaryn_make_wrapper(octaryn_deps_fastnoise2)
  endif()
endfunction()

function(octaryn_add_physics_dependencies)
  octaryn_add_package(
    NAME JoltPhysics
    GITHUB_REPOSITORY jrouwe/JoltPhysics
    VERSION 5.5.0
    SOURCE_SUBDIR Build
    OPTIONS "GENERATE_DEBUG_SYMBOLS OFF" "INTERPROCEDURAL_OPTIMIZATION OFF" "CPP_RTTI_ENABLED ON")
  if(TARGET Jolt)
    octaryn_try_link_wrapper(octaryn_deps_jolt TARGET_NAME Jolt)
  else()
    octaryn_make_wrapper(octaryn_deps_jolt)
  endif()
endfunction()

function(octaryn_add_audio_dependencies)
  set(octaryn_openal_options
    "ALSOFT_UTILS OFF"
    "ALSOFT_EXAMPLES OFF"
    "ALSOFT_TESTS OFF"
    "LIBTYPE ${OCTARYN_OPENAL_LIBTYPE}")
  if(WIN32)
    list(APPEND octaryn_openal_options
      "ALSOFT_BACKEND_PIPEWIRE OFF"
      "ALSOFT_BACKEND_PULSEAUDIO OFF"
      "ALSOFT_BACKEND_JACK OFF"
      "ALSOFT_BACKEND_PORTAUDIO OFF"
      "ALSOFT_BACKEND_ALSA OFF"
      "ALSOFT_BACKEND_OSS OFF")
  endif()
  octaryn_add_package(
    NAME OpenALSoft
    GITHUB_REPOSITORY kcat/openal-soft
    GIT_TAG 1.25.1
    OPTIONS ${octaryn_openal_options})
  if(TARGET OpenAL)
    octaryn_mark_target_system(OpenAL)
    octaryn_silence_external_warning(OpenAL -Wno-deprecated-literal-operator)
    octaryn_try_link_wrapper(octaryn_deps_openal TARGET_NAME OpenAL)
  elseif(TARGET OpenAL::OpenAL)
    octaryn_mark_target_system(OpenAL::OpenAL)
    octaryn_silence_external_warning(OpenAL::OpenAL -Wno-deprecated-literal-operator)
    octaryn_try_link_wrapper(octaryn_deps_openal TARGET_NAME OpenAL::OpenAL)
  else()
    octaryn_make_wrapper(octaryn_deps_openal)
  endif()

  octaryn_add_package(
    NAME miniaudio
    GITHUB_REPOSITORY mackron/miniaudio
    GIT_TAG 0.11.25)
  if(TARGET miniaudio)
    octaryn_try_link_wrapper(octaryn_deps_miniaudio TARGET_NAME miniaudio)
  elseif(EXISTS "${miniaudio_SOURCE_DIR}")
    octaryn_make_header_only_wrapper(octaryn_deps_miniaudio "${miniaudio_SOURCE_DIR}")
  else()
    octaryn_make_wrapper(octaryn_deps_miniaudio)
  endif()
endfunction()

function(octaryn_add_diagnostics_dependencies)
  octaryn_add_package(
    NAME cpptrace
    GITHUB_REPOSITORY jeremy-rifkin/cpptrace
    VERSION 1.0.4
    OPTIONS
      "CPPTRACE_BUILD_TESTING OFF"
      "CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF OFF"
      "CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE ON"
      "CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH ON"
      "BUILD_SHARED_LIBS OFF")
  octaryn_try_link_wrapper(octaryn_deps_cpptrace TARGET_NAME cpptrace::cpptrace)
endfunction()

function(octaryn_add_tracy_dependency)
  octaryn_add_package(
    NAME tracy
    GITHUB_REPOSITORY wolfpld/tracy
    VERSION 0.13.1
    OPTIONS
      "TRACY_ENABLE ON"
      "TRACY_ON_DEMAND ON"
      "TRACY_NO_CALLSTACK ON"
      "TRACY_NO_SAMPLING ON"
      "TRACY_NO_SYSTEM_TRACING ON"
      "TRACY_NO_FRAME_IMAGE ON")
  octaryn_try_link_wrapper(octaryn_deps_tracy TARGET_NAME Tracy::TracyClient)
endfunction()

function(octaryn_configure_dependencies)
  if(NOT TARGET octaryn_deps_spdlog)
    octaryn_add_core_dependencies()
  endif()

  if(NOT TARGET octaryn_deps_zlib OR NOT TARGET ZLIB::ZLIB)
    octaryn_add_zlib_dependency()
  endif()

  if(NOT TARGET octaryn_deps_sdl3_image)
    octaryn_add_sdl3_image_dependency()
  endif()

  if(NOT TARGET octaryn_deps_fastnoise2)
    octaryn_add_worldgen_dependencies()
  endif()

  if(NOT TARGET octaryn_deps_taskflow)
    octaryn_add_taskflow_dependency()
  endif()

  if(OCTARYN_ENABLE_RENDERING AND OCTARYN_ENABLE_SDL_GPU)
    if(NOT TARGET octaryn_deps_imgui)
      octaryn_add_imgui_dependency()
    endif()
    if(NOT TARGET octaryn_deps_implot OR NOT TARGET octaryn_deps_implot3d OR NOT TARGET octaryn_deps_imgui_node_editor OR
       NOT TARGET octaryn_deps_imguizmo OR NOT TARGET octaryn_deps_imanim OR NOT TARGET octaryn_deps_imfiledialog)
      octaryn_add_imgui_extension_dependencies()
    endif()
  else()
    octaryn_make_wrapper(octaryn_deps_imgui)
    octaryn_make_wrapper(octaryn_deps_implot)
    octaryn_make_wrapper(octaryn_deps_implot3d)
    octaryn_make_wrapper(octaryn_deps_imgui_node_editor)
    octaryn_make_wrapper(octaryn_deps_imguizmo)
    octaryn_make_wrapper(octaryn_deps_imanim)
    octaryn_make_wrapper(octaryn_deps_imfiledialog)
  endif()

  if(NOT TARGET octaryn_deps_lz4 OR NOT TARGET octaryn_deps_zstd)
    octaryn_add_storage_dependencies()
  endif()

  if(OCTARYN_ENABLE_RUNTIME_ASSET_GENERATION)
    if(NOT TARGET octaryn_deps_shadercross)
      octaryn_add_shadercross_dependency()
    endif()
    if(NOT TARGET octaryn_deps_shaderc)
      octaryn_add_shaderc_dependency()
    endif()
    if(NOT TARGET octaryn_deps_spirv_tools)
      octaryn_add_spirv_tools_dependency()
    endif()
    if(NOT TARGET octaryn_deps_spirv_cross)
      octaryn_add_spirv_cross_dependency()
    endif()
  else()
    octaryn_make_wrapper(octaryn_deps_shadercross)
    octaryn_make_wrapper(octaryn_deps_shaderc)
    octaryn_make_wrapper(octaryn_deps_spirv_tools)
    octaryn_make_wrapper(octaryn_deps_spirv_cross)
  endif()

  if(OCTARYN_ENABLE_UI_TEXT)
    if(NOT TARGET octaryn_deps_sdl3_ttf)
      octaryn_add_ui_dependencies()
    endif()
  else()
    octaryn_make_wrapper(octaryn_deps_sdl3_ttf)
  endif()

  if(OCTARYN_ENABLE_ASSET_PIPELINE AND (NOT TARGET octaryn_deps_sdl3_image OR NOT TARGET octaryn_deps_fastgltf OR NOT TARGET octaryn_deps_ktx OR NOT TARGET octaryn_deps_meshoptimizer))
    octaryn_add_asset_dependencies()
  endif()

  if(OCTARYN_ENABLE_ANIMATION AND NOT TARGET octaryn_deps_ozz_animation)
    octaryn_add_animation_dependencies()
  else()
    octaryn_make_wrapper(octaryn_deps_ozz_animation)
  endif()

  if(OCTARYN_ENABLE_PHYSICS AND NOT TARGET octaryn_deps_jolt)
    octaryn_add_physics_dependencies()
  endif()

  if(OCTARYN_ENABLE_AUDIO AND (NOT TARGET octaryn_deps_openal OR NOT TARGET octaryn_deps_miniaudio))
    octaryn_add_audio_dependencies()
  endif()

  if(OCTARYN_ENABLE_CRASH_DIAGNOSTICS AND NOT TARGET octaryn_deps_cpptrace)
    octaryn_add_diagnostics_dependencies()
  endif()

  if((OCTARYN_ENABLE_TRACY OR OCTARYN_ENABLE_PROFILING) AND NOT TARGET octaryn_deps_tracy)
    octaryn_add_tracy_dependency()
  else()
    octaryn_make_wrapper(octaryn_deps_tracy)
  endif()

  octaryn_register_dependency_aliases()
endfunction()
