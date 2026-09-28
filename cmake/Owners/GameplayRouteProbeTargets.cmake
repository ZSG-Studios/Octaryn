add_executable(octaryn_gameplay_route_probe EXCLUDE_FROM_ALL
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/GameplayRouteProbe.cpp")
target_include_directories(octaryn_gameplay_route_probe PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-client/Source/App/Validation")
target_link_libraries(octaryn_gameplay_route_probe PRIVATE octaryn::deps::glaze)
target_compile_features(octaryn_gameplay_route_probe PRIVATE cxx_std_23)
octaryn_apply_owner_layout(octaryn_gameplay_route_probe tools)
