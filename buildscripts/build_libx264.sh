#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later AND MIT
#     Copyright (c) 2022 Zoff

set -euo pipefail

readonly SCRIPT_DIR="$(dirname "$(realpath "$0")")"

source "${SCRIPT_DIR}/build_utils.sh"

parse_arch --dep "libx264" --supported "macos" "$@"

"${SCRIPT_DIR}/download/download_libx264.sh"

LDFLAGS="-fstack-protector ${CROSS_LDFLAG}" \
CFLAGS="-O2 -g0 ${CROSS_CFLAG}" \
    ./configure "${HOST_OPTION}" \
                            "--prefix=${DEP_PREFIX}" \
                            --disable-opencl \
                            --enable-static \
                            --disable-avs \
                            --disable-cli \
                            --disable-asm \
                            --enable-pic

make -j "${MAKE_JOBS}"
make install
