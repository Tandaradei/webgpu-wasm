#!/bin/bash
pushd src/shaders
glslc standard.vert -o compiled/standard.vert.spv
glslc standard.frag -o compiled/standard.frag.spv
popd