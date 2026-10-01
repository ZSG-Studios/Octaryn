add_executable(octaryn_scheduler_reuse_probe EXCLUDE_FROM_ALL
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/SchedulerReuseProbe.cpp")
target_link_libraries(octaryn_scheduler_reuse_probe PRIVATE octaryn_native_jobs)
target_compile_features(octaryn_scheduler_reuse_probe PRIVATE cxx_std_20)
octaryn_apply_owner_layout(octaryn_scheduler_reuse_probe tools)

add_executable(octaryn_authority_ordering_probe EXCLUDE_FROM_ALL
    "${OCTARYN_WORKSPACE_ROOT_DIR}/tools/validation/AuthorityOrderingProbe.cpp"
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Tick/AuthorityTick.cpp")
target_include_directories(octaryn_authority_ordering_probe PRIVATE
    "${OCTARYN_WORKSPACE_ROOT_DIR}/octaryn-server/Source/Tick")
target_link_libraries(octaryn_authority_ordering_probe PRIVATE octaryn_native_jobs)
target_compile_features(octaryn_authority_ordering_probe PRIVATE cxx_std_20)
octaryn_apply_owner_layout(octaryn_authority_ordering_probe tools)
