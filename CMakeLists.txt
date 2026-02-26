cmake_minimum_required(VERSION 3.18)

# Toolchain must be set before project() call.
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain-arm-none-eabi-gcc.cmake)
endif()

project(coralcubedflight)

include(coralmicro/third_party/github/cristianadam/bundle_static_library.cmake)

if (NOT DEFINED CMAKE_BUILD_TYPE OR NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build Type" FORCE)
endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(.)

if (${CMAKE_SOURCE_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
    add_subdirectory(coralmicro/apps)
    add_subdirectory(coralmicro/examples)
else()
    add_subdirectory(coralmicro/apps/elf_loader)
endif()

add_subdirectory(libs)