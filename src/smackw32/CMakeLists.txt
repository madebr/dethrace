add_library(smackw32 STATIC)

target_include_directories(smackw32
    PUBLIC
        include
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(smackw32 PRIVATE harness brender libsmacker compile_with_werror)

if(NOT MSVC)
    add_compile_flags_if_supported(smackw32
        -Wstrict-prototypes
    )
else()
    target_compile_definitions(smackw32 PRIVATE -D_CRT_SECURE_NO_WARNINGS)
    target_compile_options(smackw32 PRIVATE
        /wd4101
        /wd4996
    )
endif()

target_sources(smackw32 PRIVATE
    smackw32.c
)
