#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_DIR:-"$root/../ziran"}
compiler=${ZI2C_BIN:-"$ziran/build/bin/zi2c"}
ziran_bin=${ZIRAN_BIN:-"$(dirname "$compiler")/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$compiler" --no-main --root "$root" \
    --module-path "$ziran/std" -o "$work/generated" \
    "$root/client.zi" "$root/transaction.zi" "$root/sync.zi" \
    "$root/social.zi" "$root/account.zi" "$root/events.zi"

for test in wire auth url client transaction sync social account events; do
    "${CC:-cc}" -std=c11 -O0 -Wall -Wextra -Werror \
        -Wno-unused-function -Wno-unused-variable \
        -I"$ziran/include" -I"$work/generated" \
        "$root/tests/${test}_test.c" \
        "$work/generated/auth.c" "$work/generated/client.c" \
        "$work/generated/url.c" "$work/generated/wire.c" \
        "$work/generated/transaction.c" "$work/generated/sync.c" \
        "$work/generated/social.c" "$work/generated/account.c" \
        "$work/generated/events.c" \
        "$work/generated/json_scan.c" "$work/generated/text_buffer.c" \
        "$work/generated/text.c" \
        -o "$work/${test}_test"
    env -u DISPLAY -u WAYLAND_DISPLAY "$work/${test}_test"
done

"$ziran_bin" bundle --root "$root/tests" \
    --module-path "$root" --module-path "$ziran/std" \
    --entry url_portable:Check -o "$work/url.zib" \
    "$root/tests/url_portable.zi"
test "$("$ziran_bin" run "$work/url.zib")" = 0
echo 'Daochi Ziran URL portable bundle passed'
