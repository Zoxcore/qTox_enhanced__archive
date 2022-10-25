#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later AND MIT
#    Copyright © 2019-2021 by The qTox Project Contributors

set -euo pipefail

"$(dirname "$(realpath "$0")")/download/download_nasm.sh"

./autogen.sh
./configure --prefix=/

make -j $(nproc)

# seems man pages are not always built. but who needs those
touch nasm.1
touch ndisasm.1
make install

type -a nasm

nasm --version
