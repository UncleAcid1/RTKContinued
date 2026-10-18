#!/bin/sh
# testrun.sh SCRATCH BASE NAME FRAMES [CLICKS] [LINES]: the test loop (needs the test hook ON:
# python3 tools/test_hook.py on tools/test_hook.txt, and OFF again before any commit).
# Copies the storage dir SCRATCH/BASE to SCRATCH/NAME (never the real save), runs FRAMES frames with
# the taps CLICKS ("frame:x:y,..." at 1280x800), writes SCRATCH/NAME.png and NAME.log, and prints the
# tutorial-step changes. Env: SL=frames to sleep 1.1 s at (jobs, hovers, speed-ups run on wall-clock
# seconds), CO=frames to collect one drop at (drops land at random spots).
S="$1"
cd "$(dirname "$0")/.."
rm -rf "$S/$3"; cp -R "$S/$2" "$S/$3"
RTK_T=none RTK_NOCLOSE=1 RTK_SLEEPS="$SL" RTK_COLLECT="$CO" RTK_CLICK="$5" ./build/rtk --root .. \
    --storage "$S/$3" --screenshot "$S/$3.png" --frames "$4" > "$S/$3.log" 2>&1
grep "\[t\] f=" "$S/$3.log" | awk '{print $2, $3, $5, $6, $7}' | uniq -f1 | tail -"${6:-8}"
