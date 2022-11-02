#!/bin/bash

# Usage:
#   ./tools/$script_name

set -eu -o pipefail

cd translations/
for i in $(ls -1 *.ts); do
    echo sed -i -e 's#<translation type="unfinished">\([^<]\)#<translation>\1#g' "$i"
    sed -i -e 's#<translation type="unfinished">\([^<]\)#<translation>\1#g' "$i"
done
