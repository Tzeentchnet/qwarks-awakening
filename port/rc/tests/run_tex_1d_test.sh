#!/usr/bin/env bash
# Host test for R&C patch 7045 (1D textures as width x 1 2D textures, V clamped). Usage: port/rc/tests/run_tex_1d_test.sh [patched tree]
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
PT=${1:-"$here/../../../work/ps3recomp-patched"}
bin="${TMPDIR:-/tmp}/test_tex_1d_$$"
cc -std=c11 -O1 -Wall -Wextra -I"$PT/libs/video" "$here/test_tex_1d.c" -lm -o "$bin"
"$bin"
rm -f "$bin"
