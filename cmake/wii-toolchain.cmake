# Toolchain definition for devkitPro's devkitPPC Wii environment.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR powerpc)

if(NOT DEFINED ENV{DEVKITPPC})
    message(FATAL_ERROR "DEVKITPPC is not set. Use the provided Wii development container.")
endif()

set(DEVKITPPC "$ENV{DEVKITPPC}")
set(CMAKE_CXX_COMPILER "${DEVKITPPC}/bin/powerpc-eabi-g++")
set(CMAKE_AR "${DEVKITPPC}/bin/powerpc-eabi-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${DEVKITPPC}/bin/powerpc-eabi-ranlib" CACHE FILEPATH "")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH "$ENV{DEVKITPRO}/libogc")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(WII_COMMON_FLAGS "-DGEKKO -D__wii__ -mrvl -mcpu=750 -meabi -mhard-float -mregnames")
# Force the platform flags because CMake caches the initial values on the first
# configure. This keeps an existing build directory compatible after toolchain updates.
set(CMAKE_CXX_FLAGS "${WII_COMMON_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${WII_COMMON_FLAGS}" CACHE STRING "" FORCE)
