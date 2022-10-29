#!/bin/bash

#    Copyright © 2021 by The qTox Project Contributors
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

# use toxcore with enhanced ToxAV
TOXCORE_VERSION="367fd1ee5a7a9c407154baa75036c06d1e90709b" # 0.2.18 enhanced
TOXCORE_HASH="27e7ac406d7e2a4cc586dce2c0b546ef649135c807c131565c99163f45a61e30"

source "$(dirname "$(realpath "$0")")/common.sh"

download_verify_extract_tarball \
    https://github.com/zoff99/c-toxcore/archive/"$TOXCORE_VERSION".tar.gz \
    "$TOXCORE_HASH"
