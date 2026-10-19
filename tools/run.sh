#!/bin/bash
# Test run: start the game, stop it after SECS seconds (default 30), show the end of the log and a symbolized guest
# backtrace. Silent and hidden by default (EOWRECOMP_NO_AUDIO=0 / SHOW=1 to change that). Test runs use a scratch
# save folder (build/testsave, emptied first unless KEEPSAVE=1) and never touch save/.
# Extra EOWRECOMP_* variables pass through. Linux without system packages: build/deps-linux/lib is on the library path.
cd "$(dirname "$0")/.."
SECS=${SECS:-30}
export EOWRECOMP_NO_AUDIO=${EOWRECOMP_NO_AUDIO:-1} EOWRECOMP_NO_MOVIE_AUDIO=${EOWRECOMP_NO_MOVIE_AUDIO:-1}
export EOWRECOMP_NO_LAUNCHER=1
[ -z "$SHOW" ] && export EOWRECOMP_HIDDEN=1
[ -d build/deps-linux/lib ] && export LD_LIBRARY_PATH="$PWD/build/deps-linux/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
[ -z "$KEEPSAVE" ] && rm -rf build/testsave
mkdir -p build/testsave
export EOWRECOMP_SAVE_DIR=build/testsave
LOG=${LOG:-build/run.log}
${RUNPREFIX} ./build/eowrecomp extracted > "$LOG" 2>&1 &
PID=$!
for i in $(seq 1 "$SECS"); do sleep 1; kill -0 $PID 2>/dev/null || break; done
kill -9 $PID 2>/dev/null && echo "(still running after ${SECS}s, killed)"
wait $PID 2>/dev/null
grep -v "^\[svc\]" "$LOG" | grep -v "host backtrace" | tail -"${TAIL:-40}"
ADDRS=$(grep -oE "f_[0-9a-f]{10}" "$LOG" | head -"${DEPTH:-14}" | sed 's/f_//' | tr '\n' ' ')
[ -n "$ADDRS" ] && (cd tools && ./venv/bin/python sym.py $ADDRS)
exit 0
