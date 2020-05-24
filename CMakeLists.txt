cmake_minimum_required(VERSION 3.7)
project(spider)

set(CMAKE_C_COMPILER "emcc")

SET(BASEPATH "${CMAKE_SOURCE_DIR}")

file(GLOB SHADERS
    ${BASEPATH}/src/shaders/*.vert
    ${BASEPATH}/src/shaders/*.frag
    ${BASEPATH}/src/shaders/*.comp
)

set(COMPILED_SHADERS "")

foreach(SHADER ${SHADERS})
    get_filename_component(FILENAME ${SHADER} NAME)
    SET(COMPILED_SHADER "${BASEPATH}/src/shaders/compiled/${FILENAME}.spv")
    add_custom_command(OUTPUT ${COMPILED_SHADER}
    COMMAND glslc ${SHADER} -o ${COMPILED_SHADER}
    DEPENDS ${SHADER}
    COMMENT "Rebuilding ${FILENAME}.spv" )
    message(STATUS "Generating build commands for ${FILENAME}.spv")
    list(APPEND COMPILED_SHADERS ${COMPILED_SHADER})
endforeach()

set(SOURCE_FILES 
    src/example.c
    src/spider/camera.c
    src/spider/color.c
    src/spider/file.c
    src/spider/gltf_load.c
    src/spider/instance.c
    src/spider/light.c
    src/spider/material.c
    src/spider/mesh.c
    src/spider/state.c
    src/spider/ubos.c
)

set(EMCC_LINKER_FLAGS "-s USE_WEBGPU=1 -s WASM=1 -s ASSERTIONS=1 -s ALLOW_MEMORY_GROWTH=1 --preload-file ${BASEPATH}/src/shaders/compiled@/src/shaders/compiled --preload-file ${BASEPATH}/assets@/assets")
set(CMAKE_REQUIRED_FLAGS "${EMCC_LINKER_FLAGS}")

include_directories(
    external/emscripten/system/include
    external/cglm/include
    external/stb
    external/cgltf
)

if(${CMAKE_BUILD_TYPE} STREQUAL "")
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BASEPATH}/out/unnamed")
else()
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BASEPATH}/out/${CMAKE_BUILD_TYPE}")
endif()

SET(CMAKE_EXECUTABLE_SUFFIX ".html")

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${SHADERS} ${COMPILED_SHADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "${EMCC_LINKER_FLAGS}")

target_link_libraries(${PROJECT_NAME})