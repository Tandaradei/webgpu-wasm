#!/bin/bash
emcc src/example.c -I external/emscripten/system/include/ -I external/cglm/include -s USE_WEBGPU=1 -s WASM=1 -o hello.html --preload-file src/shaders/compiled