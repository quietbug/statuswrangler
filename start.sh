#!/bin/zsh
set -e

RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp}"
[[ -d "$RUNTIME_DIR" && -w "$RUNTIME_DIR" ]] || {
    print -u2 "Runtime directory is not writable: $RUNTIME_DIR"
    exit 1
}
PIPE_DIR=$(mktemp -d "$RUNTIME_DIR/.status.pipes.XXXXXX")
typeset -a child_pids=()

cleanup() {
    if (( ${#child_pids} > 0 )); then
        kill -TERM $child_pids 2>/dev/null || true
    fi
    rm -rf "$PIPE_DIR"
}

trap cleanup EXIT INT TERM HUP

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
