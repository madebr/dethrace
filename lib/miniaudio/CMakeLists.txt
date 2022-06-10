add_library(miniaudio STATIC
    miniaudio.c
)

target_include_directories(miniaudio 
    PUBLIC
        include
    PRIVATE
        include/miniaudio
)

target_compile_definitions(miniaudio PUBLIC
    MA_NO_ENCODING
    MA_NO_GENERATION
    MA_NO_RESOURCE_MANAGER
)
