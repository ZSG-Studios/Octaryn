include_guard(GLOBAL)

include(Owners/DotNetOwner)
include(Owners/NativeOwner)
include(Dependencies/ToolDependencies)

add_custom_target(octaryn_tools)

include(Owners/ToolTargets/ToolBuildPaths)
include(Owners/ToolTargets/ToolNativeTargets)
include(Owners/ToolTargets/ToolDebugStagingTargets)
include(Owners/ToolTargets/ToolCmakeValidationTargets)
include(Owners/ToolTargets/ToolModuleValidationTargets)
include(Owners/ToolTargets/ToolBundleValidationTargets)
include(Owners/ToolTargets/ToolOwnerProbeValidationTargets)
include(Owners/ToolTargets/ToolAggregateTargets)
