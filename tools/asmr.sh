#!/bin/bash
# Print annotated disassembly for an address range (hex, Ghidra addresses), ignoring function splits:
# asmr.sh 178490 178600
A="$(dirname "$0")/../out/asm_all.txt"
awk -v s="$(printf '%08x' 0x$1)" -v e="$(printf '%08x' 0x$2)" '
/^==== /{ a = substr($2,1,8) ""; if (a >= s"" && a < e"") print; next }
{ a = $1 ""; if (a >= s"" && a < e"") print }' "$A"
