#!/bin/bash
emcc src/example.c \
-I external/emscripten/system/include/ \
-I external/cglm/include \
-I external/stb \
-I external/cgltf \
-s USE_WEBGPU=1 \
-s WASM=1 \
-s ASSERTIONS=1 \
-s ALLOW_MEMORY_GROWTH=1 \
-g \
-o out/hello.html \
--preload-file src/shaders/compiled \
--preload-file assets \