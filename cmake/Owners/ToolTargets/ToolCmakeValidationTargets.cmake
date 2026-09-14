add_custom_target(octaryn_validate_cmake_targets
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_cmake_target_inventory.py"
        --build-dir "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_cmake_policy_separation
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_cmake_policy_separation.py"
        --repo-root "${OCTARYN_WORKSPACE_ROOT_DIR}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_cmake_dependency_aliases
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_cmake_dependency_aliases.py"
        --repo-root "${OCTARYN_WORKSPACE_ROOT_DIR}"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)

add_custom_target(octaryn_validate_package_policy_sync
    COMMAND "${Python3_EXECUTABLE}"
        "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/validate_package_policy_sync.py"
    WORKING_DIRECTORY "${OCTARYN_WORKSPACE_ROOT_DIR}"
    VERBATIM)
