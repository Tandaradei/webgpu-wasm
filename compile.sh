#!/bin/bash
emcc src/main.c -s USE_WEBGPU=1 -s WASM=1 -o hello.html
