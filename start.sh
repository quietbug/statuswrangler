#!/bin/zsh
set -e

PIPE_DIR="/tmp/.status.pipes"
typeset -a child_pids=()

cleanup() {
    if (( ${#child_pids} > 0 )); then
        kill -TERM $child_pids 2>/dev/null || true
    fi
    rm -rf "$PIPE_DIR"
}

trap cleanup EXIT INT TERM HUP

# remove stale dir if it exists
[[ -d "$PIPE_DIR" ]] && rm -rf "$PIPE_DIR"

mkdir -p "$PIPE_DIR"

multicat -n 8 -p "$PIPE_DIR" -h -s | sb-setroot &
child_pids+=($!)

sb-mpd -l 28         > "$PIPE_DIR/pipe1" &
child_pids+=($!)
sb-net               > "$PIPE_DIR/pipe2" &
child_pids+=($!)
xkb-switch -W        > "$PIPE_DIR/pipe3" &
child_pids+=($!)
sb-uptime            > "$PIPE_DIR/pipe4" &
child_pids+=($!)
sb-volmon            > "$PIPE_DIR/pipe5" &
child_pids+=($!)
sb-cpuload -d 1 -t 3 > "$PIPE_DIR/pipe6" &
child_pids+=($!)
sb-memwatch          > "$PIPE_DIR/pipe7" &
child_pids+=($!)
sb-timedate          > "$PIPE_DIR/pipe8" &
child_pids+=($!)

wait
