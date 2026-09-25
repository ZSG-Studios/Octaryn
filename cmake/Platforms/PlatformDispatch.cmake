include_guard(GLOBAL)

set(OCTARYN_HOST_PLATFORM "${CMAKE_HOST_SYSTEM_NAME}")
set(OCTARYN_TARGET_PLATFORM "${CMAKE_SYSTEM_NAME}")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(Platforms/Linux/LinuxPlatform)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    include(Platforms/Windows/WindowsPlatform)
else()
    message(FATAL_ERROR "Unsupported Octaryn target platform ${CMAKE_SYSTEM_NAME}. Active platforms are Linux and Windows.")
endif()
