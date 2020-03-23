#!/bin/bash
# -s ALLOW_MEMORY_GROWTH=1 
emcc src/example.c -I external/emscripten/system/include/ -I external/cglm/include -s USE_WEBGPU=1 -s WASM=1 -s ASSERTIONS=1 -s SAFE_HEAP=1 -fsanitize=undefined -g -o hello.html --preload-file src/shaders/compiled