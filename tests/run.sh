#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_DIR:-"$root/../ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$ziran/build/bin/zi2c" --no-main --root "$root" \
    --module-path "$ziran/std" -o "$work/generated" \
    "$root/client.zi"

for test in wire auth url client; do
    "${CC:-cc}" -std=c11 -O0 -Wall -Wextra -Werror \
        -Wno-unused-function -Wno-unused-variable \
        -I"$ziran/include" -I"$work/generated" \
        "$root/tests/${test}_test.c" \
        "$work/generated/auth.c" "$work/generated/client.c" \
        "$work/generated/url.c" "$work/generated/wire.c" \
        "$work/generated/json_scan.c" "$work/generated/byte_text_linux.c" \
        "$work/generated/text.c" \
        -o "$work/${test}_test"
    env -u DISPLAY -u WAYLAND_DISPLAY "$work/${test}_test"
done
