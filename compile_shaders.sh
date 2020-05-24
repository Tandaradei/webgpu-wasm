#!/bin/bash
pushd src/shaders
for f in *.vert ; 
    do glslc "$f" -o compiled/"${f}.spv" ;
    echo "compiled $f"
done
for f in *.frag ; 
    do glslc "$f" -o compiled/"${f}.spv" ;
    echo "compiled $f"
done
for f in *.comp ; 
    do glslc "$f" -o compiled/"${f}.spv" ;
    echo "compiled $f"
done
popd