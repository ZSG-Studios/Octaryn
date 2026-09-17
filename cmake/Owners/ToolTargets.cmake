include_guard(GLOBAL)

include(Owners/DotNetOwner)
include(Owners/NativeOwner)

add_custom_target(octaryn_tools)

include(Owners/ToolTargets/ToolBuildPaths)
include(Owners/ToolTargets/ToolNativeTargets)
include(Owners/ToolTargets/ToolVoxelTraceQualification)
include(Owners/ToolTargets/ToolCmakeValidationTargets)
include(Owners/ToolTargets/ToolModuleValidationTargets)
include(Owners/ToolTargets/ToolBundleValidationTargets)
include(Owners/ToolTargets/ToolOwnerProbeValidationTargets)
include(Owners/ToolTargets/ToolAggregateTargets)
