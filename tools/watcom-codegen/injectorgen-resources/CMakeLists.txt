
cmake_minimum_required(VERSION 3.15)
project(carm95hooks C CXX)

if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    conan_basic_setup(TARGETS NO_OUTPUT_DIRS)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(TestBigEndian)

test_big_endian(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    message(FATAL_ERROR "x86 is supposed to be little endian")
endif()

find_package(Detours REQUIRED)

option(HOOK_WEBSERVER "Create webserver")
if(HOOK_WEBSERVER)
    find_package(Ulfius REQUIRED)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/hook_sources.cmake")
add_library(carm95hooks SHARED
    carm95_main.cpp
    include/carm95_hooks.h
    carm95_hooks.cpp
    include/carm95_webserver.h
    carm95_webserver.cpp
    include/carm95_libc.h
    carm95_libc.c
    carm95_s3sound.c
    carm95_dummy.c
    harness_trace.c
    ${HOOK_SOURCES}
)
target_compile_definitions(carm95hooks PRIVATE
    PLUGIN_NAME="carm95hooks"
    BR_ENDIAN_BIG=$<BOOL:${IS_BIG_ENDIAN}>
)
target_compile_options(carm95hooks
    PRIVATE
        -Wall
)
if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    target_compile_definitions(carm95hooks
        PRIVATE
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
    )
    target_compile_options(carm95hooks
        PRIVATE
            -wd4820  # x' bytes padded
            -wd4255  # no function prototype given: converting '()' to '(void)'
            -wd4101  # unreferenced local variable
            -wd4774  # 'printf' : format string expected in argument 1 is not a string literal
            -wd5247  # section '.CRT$XCZ' is reserved for C++ dynamic initialization
            -wd5045  # Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
            -wd4242  # function': conversion from 'xxx' to 'yyy', possible loss of data
            -wd4244  # function': conversion from 'xxx' to 'yyy', possible loss of data
    )
endif()
target_include_directories(carm95hooks PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/BRSRC13
   ${CMAKE_CURRENT_SOURCE_DIR}/DETHRACE/source
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/include/harness
   ${CMAKE_CURRENT_SOURCE_DIR}/include/brender
)
target_link_libraries(carm95hooks PRIVATE
    Detours::Detours
)
set_target_properties(carm95hooks
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME carm95hooks
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
if(MSVC)
    target_sources(carm95hooks PRIVATE hook_msvc.def)
elseif(MINGW)
    target_sources(carm95hooks PRIVATE hook_mingw.def)
endif()
target_compile_definitions(carm95hooks PRIVATE HOOK_WEBSERVER=$<BOOL:${HOOK_WEBSERVER}>)
if(HOOK_WEBSERVER)
    target_link_libraries(carm95hooks PRIVATE Ulfius::Ulfius-static)
endif()

add_executable(carm95_injector
    injector_main.cpp
    injector_hasher.c
    injector.h
)
target_compile_definitions(carm95_injector PRIVATE
    TARGET="carm95_injector"
    VICTIM="CARM95.EXE"
)
target_link_libraries(carm95_injector PRIVATE
    bcrypt
    Detours::Detours
)
set_target_properties(carm95_injector
    PROPERTIES
        CXX_STANDARD 17
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set(CARM95_LOCATION "" CACHE FILEPATH "Path of CARM95.EXE (+game data)")
if(CARM95_LOCATION)
    add_custom_target(copy_hooks
        COMMENT "Copying carm95 hooks to \"${CARM95_LOCATION}\""
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CARM95_LOCATION}/plugins"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:carm95hooks>" "${CARM95_LOCATION}/plugins/$<TARGET_FILE_NAME:carm95hooks>"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:carm95_injector>" "${CARM95_LOCATION}/$<TARGET_FILE_NAME:carm95_injector>"
    )
endif()
