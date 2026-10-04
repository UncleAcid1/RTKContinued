#!/bin/bash
# Print decompiled C for functions whose full name matches a regex: fn.sh 'Map::Load$'
D="$(dirname "$0")/../out/decomp.c"
awk -v pat="$1" '/^\/\/ ==== /{name=$0; sub(/^\/\/ ==== [0-9a-f]+ /,"",name); p=(name ~ pat)} p' "$D"
