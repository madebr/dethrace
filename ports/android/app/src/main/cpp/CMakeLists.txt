cmake_minimum_required(VERSION 3.0)
project(android_dethrace C)

add_subdirectory(../../../../../.. dethrace EXCLUDE_FROM_ALL)

enable_language(CXX)
set_property(TARGET dethrace PROPERTY LINKER_LANGUAGE CXX)
set_property(TARGET dethrace PROPERTY EXCLUDE_FROM_ALL FALSE)
