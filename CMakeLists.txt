cmake_minimum_required(VERSION 3.10)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo" FORCE)
endif()

set(DETHRACE_IDE_ARGUMENTS "" CACHE STRING "DethRace arguments (only used by MSVC when debugging")
set(DETHRACE_IDE_ROOT_DIR "" CACHE PATH "DethRace rootdir (only used by MSVC when debugging)")

set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

project(dethrace C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(CMakeDependentOption)

find_package(PhysFS)

option(DETHRACE_VFS "Enable Virtual File Systems" ON)
if(PhysFS_FOUND)
    set(physfs_vendored_default FALSE)
else()
    set(physfs_vendored_default TRUE)
endif()
cmake_dependent_option(DETHRACE_PHYSFS_VENDORED "Use a vendored physfs library" "${physfs_vendored_default}" DETHRACE_VFS OFF)

option(BUILD_TESTS "Build unit tests." OFF)

find_package(SDL2 REQUIRED)
if(DETHRACE_VFS)
    add_library(dethrace_physfs INTERFACE)
    add_library(DethRace::PhysFS ALIAS dethrace_physfs)
    if(DETHRACE_PHYSFS_VENDORED)
        add_subdirectory(lib/physfs EXCLUDE_FROM_ALL)
        target_link_libraries(dethrace_physfs INTERFACE PhysFS::PhysFS-static)
    else()
        if(NOT PhysFS_FOUND)
            message(FATAL_ERROR "Could not find PhysFS")
        endif()
        if(TARGET PhysFS::PhysFS-static)
            target_link_libraries(dethrace_physfs INTERFACE PhysFS::PhysFS-static)
        elseif(TARGET PhysFS::PhysFS)
            target_link_libraries(dethrace_physfs INTERFACE PhysFS::PhysFS-static)
        elseif(DEFINED PHYSFS_LIBRARY AND DEFINED PHYSFS_INCLUDE_DIR)
            target_link_libraries(dethrace_physfs INTERFACE ${PHYSFS_LIBRARY})
            target_include_directories(dethrace_physfs INTERFACE ${PHYSFS_INCLUDE_DIR})
        endif()
    endif()
    target_sources(dethrace_physfs INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/lib/physfs/extras/ignorecase.c")
    target_include_directories(dethrace_physfs INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>")
endif()

add_subdirectory(lib/libsmacker EXCLUDE_FROM_ALL)
add_subdirectory(lib/glad EXCLUDE_FROM_ALL)
add_subdirectory(lib/miniaudio EXCLUDE_FROM_ALL)
add_subdirectory(lib/cglm EXCLUDE_FROM_ALL)

add_subdirectory(src/harness)
add_subdirectory(src/S3)
add_subdirectory(src/BRSRC13)
add_subdirectory(src/DETHRACE)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
