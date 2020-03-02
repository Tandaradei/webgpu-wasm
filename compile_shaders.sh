#!/bin/bash
pushd src/shaders
glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv
popd