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
TOXCORE_VERSION="c030ca31e5399537735a0cacac096565e42cab7e" # 0.2.18 enhanced
TOXCORE_HASH="a92f84eb8c3c0a7f0b02b711ebc0d0ce7dac41ba9b11e39cd16aa94fe599aee7"

source "$(dirname "$(realpath "$0")")/common.sh"

download_verify_extract_tarball \
    https://github.com/zoff99/c-toxcore/archive/"$TOXCORE_VERSION".tar.gz \
    "$TOXCORE_HASH"
