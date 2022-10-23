#!/bin/bash

#    Copyright © 2022 by Zoff
#
#    This program is libre software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

set -euo pipefail

source "$(dirname "$(realpath "$0")")/common.sh"

git clone https://code.videolan.org/videolan/x264.git
_X264_VERSION_="1771b556ee45207f8711744ccbd5d42a3949b14c"
LIBX264_HASH="745a2a355a4a7ecd389ec438735e2c5b5d1e4b3277352761a1c0d4f5ae4f8cd9"
cd x264/

download_verify_extract_tarball \
    "https://code.videolan.org/videolan/x264/-/archive/${_X264_VERSION_}/x264-${_X264_VERSION_}.tar.gz" \
    ${LIBX264_HASH}
