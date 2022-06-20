cmake_minimum_required(VERSION 3.0)
project(android_dethrace C)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE CACHE BOOL "Enable PIC" FORCE)
add_subdirectory(../../../../../.. dethrace EXCLUDE_FROM_ALL)

enable_language(CXX)
set_property(TARGET dethrace PROPERTY LINKER_LANGUAGE CXX)
set_property(TARGET dethrace PROPERTY EXCLUDE_FROM_ALL FALSE)
