#!/bin/zsh
set -e

PIPE_DIR="/tmp/.status.pipes"

cleanup() {
    kill 0 2>/dev/null || true
    rm -rf "$PIPE_DIR"
}

trap cleanup EXIT INT TERM HUP

# remove stale dir if it exists
[[ -d "$PIPE_DIR" ]] && rm -rf "$PIPE_DIR"

mkdir -p "$PIPE_DIR"

multicat -n 8 -p "$PIPE_DIR" -h -s | sb-setroot &

sb-mpd -l 28         > "$PIPE_DIR/pipe1" &
sb-net               > "$PIPE_DIR/pipe2" &
xkb-switch -W        > "$PIPE_DIR/pipe3" &
sb-uptime            > "$PIPE_DIR/pipe4" &
sb-volmon            > "$PIPE_DIR/pipe5" &
sb-cpuload -d 1 -t 3 > "$PIPE_DIR/pipe6" &
sb-memwatch          > "$PIPE_DIR/pipe7" &
sb-timedate          > "$PIPE_DIR/pipe8" &

wait
