include("${CMAKE_CURRENT_LIST_DIR}/../../Shared/TargetArchitecture.cmake")

if(NOT DEFINED ENV{VSCMD_ARG_TGT_ARCH})
    message(FATAL_ERROR "Native Windows builds require the Visual Studio developer environment. Use tools/build/windows.py.")
endif()
if(NOT "$ENV{VSCMD_ARG_TGT_ARCH}" STREQUAL "${OCTARYN_TARGET_ARCH}")
    message(FATAL_ERROR "Visual Studio target architecture does not match OCTARYN_TARGET_ARCH=${OCTARYN_TARGET_ARCH}.")
endif()

file(TO_CMAKE_PATH "$ENV{ProgramFiles}/LLVM/bin" octaryn_llvm_bin)
file(TO_CMAKE_PATH "$ENV{VSINSTALLDIR}/VC/Tools/Llvm/x64/bin" octaryn_vs_llvm_x64_bin)
file(TO_CMAKE_PATH "$ENV{VSINSTALLDIR}/VC/Tools/Llvm/ARM64/bin" octaryn_vs_llvm_arm64_bin)
octaryn_arch_select(octaryn_vs_llvm_bin "${octaryn_vs_llvm_x64_bin}" "${octaryn_vs_llvm_arm64_bin}")
find_program(OCTARYN_NATIVE_CLANG_CL NAMES clang-cl
    HINTS "${octaryn_llvm_bin}" "${octaryn_vs_llvm_bin}"
    REQUIRED)
set(CMAKE_C_COMPILER "${OCTARYN_NATIVE_CLANG_CL}" CACHE FILEPATH "Native Windows C compiler")
set(CMAKE_CXX_COMPILER "${OCTARYN_NATIVE_CLANG_CL}" CACHE FILEPATH "Native Windows C++ compiler")
octaryn_arch_select(octaryn_native_triple x86_64-pc-windows-msvc aarch64-pc-windows-msvc)
set(CMAKE_C_COMPILER_TARGET "${octaryn_native_triple}")
set(CMAKE_CXX_COMPILER_TARGET "${octaryn_native_triple}")
