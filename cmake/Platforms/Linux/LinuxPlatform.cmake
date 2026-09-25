include_guard(GLOBAL)
include(Shared/TargetArchitecture)

octaryn_arch_select(OCTARYN_TARGET_NATIVE_ARCHIVE_FORMAT elf64-x86-64 elf64-aarch64)
octaryn_arch_select(OCTARYN_TARGET_DOTNET_RID linux-x64 linux-arm64)
find_program(OCTARYN_TARGET_OBJDUMP NAMES llvm-objdump objdump)
