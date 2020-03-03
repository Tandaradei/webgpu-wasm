#!/bin/bash
pushd src/shaders
glslc shader.vert -o compiled/vert.spv
glslc shader.frag -o compiled/frag.spv
popd