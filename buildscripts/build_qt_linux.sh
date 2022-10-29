#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later AND MIT
#     Copyright (c) 2017-2021 Maxim Biro <nurupo.contributions@gmail.com>
#     Copyright (c) 2021 by The qTox Project Contributors

set -euo pipefail

readonly SCRIPT_DIR="$(dirname "$(realpath "$0")")"

qmake --version

"${SCRIPT_DIR}/download/download_qt.sh"

OPENSSL_LIBS=$(pkg-config --libs openssl)
export OPENSSL_LIBS

./configure -prefix "/usr" \
    -release \
    -shared \
    -openssl \
    "$(pkg-config --cflags openssl)" \
    -opensource -confirm-license \
    -pch \
    -nomake examples \
    -nomake tools \
    -nomake tests \
    -skip 3d \
    -skip activeqt \
    -skip androidextras \
    -skip canvas3d \
    -skip charts \
    -skip connectivity \
    -skip datavis3d \
    -skip declarative \
    -skip doc \
    -skip gamepad \
    -skip graphicaleffects \
    -skip imageformats \
    -skip location \
    -skip macextras \
    -skip multimedia \
    -skip networkauth \
    -skip purchasing \
    -skip quickcontrols \
    -skip quickcontrols2 \
    -skip remoteobjects \
    -skip script \
    -skip scxml \
    -skip sensors \
    -skip serialbus \
    -skip serialport \
    -skip speech \
    -skip translations \
    -skip virtualkeyboard \
    -skip webchannel \
    -skip webengine \
    -skip webglplugin \
    -skip websockets \
    -skip webview \
    -skip xmlpatterns \
    -no-compile-examples \
    -qt-libjpeg \
    -qt-libpng \
    -qt-zlib \
    -qt-pcre \
    -opengl desktop

make -j $(nproc)
make install

hash -r
qmake --version

