#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later AND MIT
#     Copyright (c) 2022 Zoff

set -euo pipefail

readonly SCRIPT_DIR="$(dirname "$(realpath "$0")")"

source "${SCRIPT_DIR}/build_utils.sh"

parse_arch --dep "libx264" --supported "win32 win64" "$@"

"${SCRIPT_DIR}/download/download_libx264.sh"

if [[ "$SCRIPT_ARCH" == "win64" ]]; then
# There is a bug in gcc that breaks avx512 on 64-bit Windows https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54412
# VPX fails to build due to it.
# This is a workaround as suggested in https://stackoverflow.com/questions/43152633
    ARCH_FLAGS="-fno-asynchronous-unwind-tables"
    CROSS_ARG="${MINGW_ARCH}-w64-mingw32-"
    TARGET_ARG="--target=x86_64-win64-gcc"
elif [[ "$SCRIPT_ARCH" == "win32" ]]; then
    ARCH_FLAGS=""
    CROSS_ARG="${MINGW_ARCH}-w64-mingw32-"
    TARGET_ARG="--target=x86-win32-gcc"
else
    exit 1
fi

CFLAGS="-O2 -g0 ${ARCH_FLAGS} ${CROSS_CFLAG}" \
LDFLAGS="-L${DEP_PREFIX}/lib/ ${CROSS_CPPFLAG}" \
LIBS="-lgdi32 -lws2_32 -lcrypto -lgdi32" \
CPPFLAGS="-I${DEP_PREFIX}/include ${CROSS_CPPFLAG}" \
CROSS="${CROSS_ARG}" \
./configure \
    --host="${MINGW_ARCH}-w64-mingw32" \
    "--prefix=${DEP_PREFIX}" \
    --disable-opencl \
    --enable-static \
    --disable-avs \
    --disable-cli \
    --disable-asm \
    --enable-pic

export CC=""
export WINDRES=""

make -j "${MAKE_JOBS}"
make install
