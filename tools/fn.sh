#!/bin/bash
# Print decompiled C for functions whose full name matches a regex: fn.sh 'Map::Load$' [file.c]
# Default source: out/decomp.c (softfp-aware export, see README); falls back to out/decomp_hardfp.c.
D="${2:-$(dirname "$0")/../out/decomp.c}"
[ -f "$D" ] || D="$(dirname "$0")/../out/decomp_hardfp.c"
awk -v pat="$1" '/^\/\/ ==== /{name=$0; sub(/^\/\/ ==== [0-9a-f]+ /,"",name); p=(name ~ pat)} p' "$D"
