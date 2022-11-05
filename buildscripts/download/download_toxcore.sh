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
TOXCORE_VERSION="201a859f0bc9484b727d523ae8850dfeda214687" # 0.2.18 enhanced
TOXCORE_HASH="f93f57c09b9ec4556726fc7686a733445ccf209b23fff554e6bc9aa00cdbbade"

source "$(dirname "$(realpath "$0")")/common.sh"

download_verify_extract_tarball \
    https://github.com/zoff99/c-toxcore/archive/"$TOXCORE_VERSION".tar.gz \
    "$TOXCORE_HASH"
