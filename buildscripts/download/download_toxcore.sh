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

# use toxcore with NGC commits from TokTok master
TOXCORE_VERSION="e0b00d3e733148823e4b63d70f464e523ad62bac" # 0.2.18
TOXCORE_HASH="d33540a5cb03d6a34bf1c9c9121d082348583e802a25f606341d832e22c9b666" # f2940537998863593e28bc6a6b5f56f09675f6cd8a28326b7bc31b4836c08942

source "$(dirname "$(realpath "$0")")/common.sh"

# download_verify_extract_tarball \
#    https://github.com/TokTok/c-toxcore/releases/download/v$TOXCORE_VERSION/c-toxcore-$TOXCORE_VERSION.tar.gz \
#    "$TOXCORE_HASH"

download_verify_extract_tarball \
    https://github.com/TokTok/c-toxcore/archive/"$TOXCORE_VERSION".tar.gz \
    "$TOXCORE_HASH"

rm -Rf third_party/cmp/
mkdir -p third_party/cmp/
cd third_party/cmp/

download_verify_extract_tarball \
    https://github.com/camgunz/cmp/archive/e836703291392aba9db92b46fb47929521fac71f.tar.gz \
    791ed2eaae05d75a6ef1c3fcad4edd595aba80a0be050067ec147d1cd94ec13a
