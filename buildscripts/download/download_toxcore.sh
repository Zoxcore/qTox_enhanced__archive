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
TOXCORE_VERSION="e94dd666a23888e38add3ea4e1481f16546fbf47" # 0.2.18 enhanced
TOXCORE_HASH="36dcf7e5e6fdb7dc581bb5741d67b4f39762348a867aca64f88aa9ab6acb627f"

source "$(dirname "$(realpath "$0")")/common.sh"

download_verify_extract_tarball \
    https://github.com/zoff99/c-toxcore/archive/"$TOXCORE_VERSION".tar.gz \
    "$TOXCORE_HASH"
