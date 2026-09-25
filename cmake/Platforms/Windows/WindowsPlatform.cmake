include_guard(GLOBAL)
include(Shared/TargetArchitecture)

octaryn_arch_select(OCTARYN_TARGET_NATIVE_ARCHIVE_FORMAT pe-x86-64 coff-arm64)
octaryn_arch_select(OCTARYN_TARGET_DOTNET_RID win-x64 win-arm64)
find_program(OCTARYN_TARGET_OBJDUMP NAMES llvm-objdump
    PATHS "$ENV{ProgramFiles}/LLVM/bin")
