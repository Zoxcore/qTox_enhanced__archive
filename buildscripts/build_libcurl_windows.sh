#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later AND MIT
#     Copyright (c) 2022 Zoff

set -euo pipefail

readonly SCRIPT_DIR="$(dirname "$(realpath "$0")")"

source "${SCRIPT_DIR}/build_utils.sh"

parse_arch --dep "libcurl" --supported "win32 win64" "$@"

"${SCRIPT_DIR}/download/download_libcurl.sh"

CFLAGS="-O2 -g0" \
LDFLAGS="-L${DEP_PREFIX}/lib/" \
LIBS="-lgdi32 -lws2_32 -lcrypto -lgdi32" \
CPPFLAGS="-I${DEP_PREFIX}/include" \
./configure \
    "${HOST_OPTION}" \
    "--prefix=${DEP_PREFIX}" \
    "--with-openssl=${DEP_PREFIX}" \
    --enable-static \
    --disable-shared

make -j "${MAKE_JOBS}"
make install
