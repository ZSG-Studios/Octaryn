add_custom_target(octaryn_validate_project_references
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_all_project_reference_boundaries.py"
        --repo-root "${OCTARYN_WORKSPACE_ROOT_DIR}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_module_manifest_packages
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_module_manifest_packages.py"
        --module-root "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame"
        --project-file "${octaryn_tool_basegame_project}"
        --allowed-package-ids-file "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-shared/Source/FrameworkAllowlist/AllowedPackageIds.cs"
        --manifest-json "${octaryn_tool_basegame_manifest_json}"
    DEPENDS
        octaryn_validate_module_manifest_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_module_manifest_files
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_module_manifest_files.py"
        --module-root "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame"
        --manifest-json "${octaryn_tool_basegame_manifest_json}"
    DEPENDS
        octaryn_validate_module_manifest_probe
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_module_manifest_probe
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ModuleManifestProbe/Octaryn.ModuleManifestProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ModuleManifestProbe/Octaryn.ModuleManifestProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
        -- "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame"
        --dump-manifest "${octaryn_tool_basegame_manifest_json}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_module_source_api
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ModuleApiProbe/Octaryn.ModuleApiProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ModuleApiProbe/Octaryn.ModuleApiProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
        -- --source-root "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame"
        --assets-file "${octaryn_tool_basegame_assets}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_module_binary_sandbox
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" restore
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ModuleBinarySandboxProbe/Octaryn.ModuleBinarySandboxProbe.csproj"
    COMMAND "${CMAKE_COMMAND}" -E env "NUGET_PACKAGES=${OCTARYN_NUGET_PACKAGES_DIR}" "OctarynBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}" "OctarynHostToolBuildPresetName=${OCTARYN_BUILD_PRESET_NAME}"
        "${DOTNET_EXECUTABLE}" run
        --project "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/Octaryn.ModuleBinarySandboxProbe/Octaryn.ModuleBinarySandboxProbe.csproj"
        --configuration "${CMAKE_BUILD_TYPE}"
        --no-restore
        -- --assembly "${tool_basegame_build_root}/managed/Octaryn.Basegame.dll"
        --assets-file "${octaryn_tool_basegame_assets}"
    DEPENDS
        octaryn_basegame
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_module_layout
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_module_layout.py"
        --module-root "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_basegame_block_catalog
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_basegame_block_catalog.py"
        --catalog "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json"
        --generated-source "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Source/Content/Blocks/BasegameBlockCatalog.cs"
        --atlas-color "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets/Atlases/basegame-color.png"
        --atlas-normal "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets/Atlases/basegame-normal.png"
        --atlas-specular "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets/Atlases/basegame-specular.png"
        --animation-atlas "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets/Atlases/basegame-animation.png"
        --animation-manifest "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Assets/Atlases/basegame-animation.txt"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_basegame_worldgen_content
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_basegame_worldgen_content.py"
        --block-catalog "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json"
        --biomes "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Data/Biomes/octaryn.basegame.biomes.json"
        --features "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Data/Features/octaryn.basegame.features.json"
        --terrain-rule "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-basegame/Data/Rules/octaryn.basegame.rule.terrain_generation.json"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_dotnet_package_assets
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_dotnet_package_assets.py"
        --assets-file "${octaryn_tool_client_assets}"
        --owner client
        --policy-file "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/package-policy/module-packages.json"
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_dotnet_package_assets.py"
        --assets-file "${octaryn_tool_server_assets}"
        --owner server
        --policy-file "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/package-policy/module-packages.json"
    COMMAND python3
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_dotnet_package_assets.py"
        --assets-file "${octaryn_tool_basegame_assets}"
        --owner basegame
        --policy-file "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/package-policy/module-packages.json"
    DEPENDS
        octaryn_client_managed
        octaryn_server
        octaryn_basegame
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
