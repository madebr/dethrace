add_library(s3 STATIC)

option(S3_FIX_BUGS "Fix S3 bugs" ON)

target_include_directories(s3 
    PUBLIC
        include
    PRIVATE
        ${CMAKE_SOURCE_DIR}      
)

target_link_libraries(s3 PRIVATE brender harness miniaudio)

if(MSVC)
    target_compile_definitions(s3 PRIVATE -D_CRT_SECURE_NO_WARNINGS)
    target_compile_options(s3 PRIVATE
        /wd4101
        /wd4996
    )
elseif(ANDROID)
    target_compile_options(s3 PRIVATE
        -Werror
    )
    target_link_libraries(s3 PUBLIC m)
else()
    target_compile_options(s3 PRIVATE
        -Werror
    )
    target_link_libraries(s3 PUBLIC pthread m)
endif()

target_sources(s3 PRIVATE
    3d.c
    3d.h
    audio.c
    audio.h
    ioserv.c
    ioserv.h
    profile.c
    profile.h
    resource.c
    resource.h
    s3cda.c
    s3cda.h
    s3music.c
    s3music.h
    s3sound.c
    s3sound.h
)

if(S3_FIX_BUGS)
    target_compile_definitions(s3 PRIVATE S3_FIX_BUGS)
endif()
