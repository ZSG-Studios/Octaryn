include_guard(GLOBAL)

find_program(DOTNET_EXECUTABLE dotnet REQUIRED)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)

set(OCTARYN_NUGET_PACKAGES_DIR
    "${OCTARYN_WORKSPACE_ROOT_DIR}/build/dependencies/nuget"
    CACHE PATH
    "Workspace-managed NuGet package cache")

set(OCTARYN_DOTNET_TARGET_RUNTIME_ARGS)
if(OCTARYN_TARGET_DOTNET_RID)
    list(APPEND OCTARYN_DOTNET_TARGET_RUNTIME_ARGS
        --runtime "${OCTARYN_TARGET_DOTNET_RID}")
endif()
