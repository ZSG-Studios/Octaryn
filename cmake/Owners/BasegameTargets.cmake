include_guard(GLOBAL)

include(Owners/DotNetOwner)
include(Owners/NativeOwner)

octaryn_add_native_owner(octaryn_basegame_native)
add_dependencies(octaryn_basegame_native octaryn_shared_native)

octaryn_add_dotnet_owner(
    octaryn_basegame
    basegame
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Octaryn.Basegame.csproj")

add_dependencies(octaryn_basegame octaryn_shared octaryn_basegame_native)

octaryn_owner_build_root(basegame_build_root basegame)
octaryn_owner_log_root(basegame_log_root basegame)
set(octaryn_basegame_bundle_dir "${basegame_build_root}/bundle")
set(octaryn_basegame_bundle_obj_dir "${basegame_build_root}/bundle-obj")
set(octaryn_basegame_bundle_stamp "${basegame_build_root}/stamps/octaryn_basegame_bundle.stamp")

add_custom_command(
    OUTPUT "${octaryn_basegame_bundle_stamp}"
    BYPRODUCTS
        "${octaryn_basegame_bundle_dir}/Octaryn.Basegame.dll"
        "${octaryn_basegame_bundle_dir}/Octaryn.Basegame.deps.json"
        "${octaryn_basegame_bundle_dir}/Octaryn.Basegame.runtimeconfig.json"
        "${octaryn_basegame_bundle_dir}/Data/Module/octaryn.basegame.module.json"
        "${octaryn_basegame_bundle_dir}/Data/Blocks/octaryn.basegame.blocks.json"
        "${octaryn_basegame_bundle_dir}/Data/Biomes/octaryn.basegame.biomes.json"
        "${octaryn_basegame_bundle_dir}/Data/Features/octaryn.basegame.features.json"
        "${octaryn_basegame_bundle_dir}/Data/Items/octaryn.basegame.item.hand.json"
        "${octaryn_basegame_bundle_dir}/Data/Rules/octaryn.basegame.rule.default_interaction.json"
        "${octaryn_basegame_bundle_dir}/Data/Rules/octaryn.basegame.rule.terrain_generation.json"
        "${octaryn_basegame_bundle_dir}/Assets/Atlases/basegame-color.png"
        "${octaryn_basegame_bundle_dir}/Assets/Atlases/basegame-normal.png"
        "${octaryn_basegame_bundle_dir}/Assets/Atlases/basegame-specular.png"
        "${octaryn_basegame_bundle_dir}/Assets/Atlases/basegame-animation.png"
        "${octaryn_basegame_bundle_dir}/Assets/Atlases/basegame-animation.txt"
        "${octaryn_basegame_bundle_dir}/Assets/Atlases/basegame-color.txt"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_basegame_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_basegame_bundle_dir}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${octaryn_basegame_bundle_obj_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${octaryn_basegame_bundle_obj_dir}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynIntermediateRoot=${octaryn_basegame_bundle_obj_dir}"
        "${DOTNET_EXECUTABLE}" restore "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Octaryn.Basegame.csproj"
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
    COMMAND "${CMAKE_COMMAND}" -E env
        "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}"
        "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_ROOT_NAME}"
        "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "OctarynIntermediateRoot=${octaryn_basegame_bundle_obj_dir}"
        "${DOTNET_EXECUTABLE}" publish "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Octaryn.Basegame.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --framework net10.0
        --output "${octaryn_basegame_bundle_dir}"
        --no-self-contained
        --no-restore
        -p:OctarynSkipModuleResolvedReferenceValidation=true
        ${OCTARYN_DOTNET_TARGET_RUNTIME_ARGS}
        "-bl:${basegame_log_root}/octaryn_basegame_bundle-${OCTARYN_BUILD_PRESET_NAME}.binlog"
    COMMAND "${CMAKE_COMMAND}" -E touch "${octaryn_basegame_bundle_stamp}"
    DEPENDS
        "${octaryn_basegame_STAMP}"
        "${octaryn_shared_STAMP}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_basegame_bundle
    DEPENDS "${octaryn_basegame_bundle_stamp}")

set(octaryn_default_game_module_target octaryn_basegame)
set(octaryn_default_game_module_stamp "${octaryn_basegame_STAMP}")
set(octaryn_default_game_module_bundle_target octaryn_basegame_bundle)
set(octaryn_default_game_module_bundle_stamp "${octaryn_basegame_bundle_stamp}")
set(octaryn_default_game_module_bundle_dir "${octaryn_basegame_bundle_dir}")

add_custom_command(
    OUTPUT "${octaryn_basegame_STAMP}"
    APPEND
    DEPENDS
        "${octaryn_shared_STAMP}")
