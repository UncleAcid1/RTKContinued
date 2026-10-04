#!/bin/bash
# Print annotated disassembly for functions whose full name matches a regex: asm.sh 'GUI::Init$'
# Source: out/asm_all.txt (DumpAsm.java '.' + pic_strings.py, see README).
A="$(dirname "$0")/../out/asm_all.txt"
awk -v pat="$1" '/^==== /{name=$3; p=(name ~ pat)} p' "$A"
