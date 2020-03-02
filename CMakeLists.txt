cmake_minimum_required(VERSION 3.13)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(hello)
set(CMAKE_C_COMPILER "emcc")
set(EMCC_LINKER_FLAGS "-o hello.html")

add_executable(hello
    "${PROJECT_SOURCE_DIR}/src/main.c"
    )

target_include_directories(hello
    PRIVATE "${PROJECT_SOURCE_DIR}/external/emscripten/system/include"
    )

target_link_options(hello
    PRIVATE "SHELL:-g1"
    PRIVATE "SHELL:-s USE_WEBGPU=1"
    PRIVATE "SHELL:-s ASSERTIONS=1"
    PRIVATE "SHELL:-s WASM=1"
    #PRIVATE "SHELL:--closure 1"
    #PRIVATE "SHELL:-s VERBOSE=1"
    )

