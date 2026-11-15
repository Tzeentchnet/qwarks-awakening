#!/usr/bin/env bash
# Builds and runs test_rc_cursor_art.c (R&C patch 7065, the wrench pointer of libs/input/pad_cursor_art.h) against the
# patched tree (RC_P3R_TREE, default work/ps3recomp-patched of THIS checkout, then the main one), then an ASan/UBSan pass.
# RC_CURSOR_PPM=FILE also writes the 48 px pointer, 4x, on grey (hotspot magenta).
#   nix develop .#re --command port/rc/tests/run_rc_cursor_art_test.sh
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../../.." && pwd)"
main="$(cd "$(git -C "$here" rev-parse --path-format=absolute --git-common-dir)/.." && pwd)"
tree="${RC_P3R_TREE:-$repo/work/ps3recomp-patched}"; [[ -f "$tree/libs/input/pad_cursor_art.h" ]] || tree="$main/work/ps3recomp-patched"
tmp="${TMPDIR:-/tmp}/rccursor_$$"; mkdir -p "$tmp"; trap 'rm -rf "$tmp"' EXIT
I=(-I"$tree/libs/input" -I"$repo/port/rc")
cc="${HOST_CC:-gcc}"
W="-std=c11 -Wall -Wextra -Werror -Wno-unused-function"
"$cc" $W -O2 "${I[@]}" "$here/test_rc_cursor_art.c" -o "$tmp/test" -lm
"$tmp/test" ${RC_CURSOR_PPM:+"$RC_CURSOR_PPM"} | grep -v '^\[PASS\]' || true
"$tmp/test" >/dev/null || { echo "[FAIL] test_rc_cursor_art"; exit 1; }
if echo "int main(void){return 0;}" | "$cc" -fsanitize=address,undefined -x c - -o "$tmp/probe" 2>/dev/null && "$tmp/probe"; then
  "$cc" $W -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all "${I[@]}" "$here/test_rc_cursor_art.c" -o "$tmp/test_asan" -lm
  "$tmp/test_asan" >/dev/null && echo "[PASS] ASan/UBSan pass" || { echo "[FAIL] ASan/UBSan pass"; exit 1; }
fi
