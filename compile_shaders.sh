#!/bin/bash
pushd src/shaders
for f in *.vert ; 
    do glslc "$f" -o compiled/"${f}.spv" ; 
done
for f in *.frag ; 
    do glslc "$f" -o compiled/"${f}.spv" ; 
done
popd