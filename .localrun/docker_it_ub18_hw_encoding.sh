#! /bin/bash

_HOME2_=$(dirname $0)
export _HOME2_
_HOME_=$(cd $_HOME2_;pwd)
export _HOME_

echo $_HOME_
cd $_HOME_


if [ "$1""x" == "buildx" ]; then
    # HINT: to show docker build logmessages better
    export BUILDKIT_PROGRESS=plain
    cp -a ../buildscripts .
    docker build -f Dockerfile_ub18_hw_encoding -t qtox_push_002_ub18_hw_encoding .
    exit 0
fi

cp -a ../.ci-scripts/build-qtox-linux.sh .

build_for='
ubuntu:18.04
'

for system_to_build_for in $build_for ; do

    system_to_build_for_orig="$system_to_build_for"
    system_to_build_for=$(echo "$system_to_build_for_orig" 2>/dev/null|tr ':' '_' 2>/dev/null)

    cd $_HOME_/
    mkdir -p $_HOME_/"$system_to_build_for"/

    mkdir -p $_HOME_/"$system_to_build_for"/artefacts
    mkdir -p $_HOME_/"$system_to_build_for"/script
    mkdir -p $_HOME_/"$system_to_build_for"/workspace

    ls -al $_HOME_/"$system_to_build_for"/

    rsync -a ../ --exclude=.localrun $_HOME_/"$system_to_build_for"/workspace/build
    chmod a+rwx -R $_HOME_/"$system_to_build_for"/workspace/build

    echo '#! /bin/bash

cp -av /workspace/build/* /qtox/
cp -av /workspace/build/.??* /qtox/
cd /qtox/.ci-scripts/

# disable tests
sed -i -e "s#^include(Testing)##" /qtox/CMakeLists.txt
cat /qtox/CMakeLists.txt|grep -i test

./build-qtox-linux.sh --full --build-type Release

ls -hal /qtox/.ci-scripts/qtox

cp -av /qtox/.ci-scripts/qtox /artefacts/
cp -av /usr/lib/x86_64-linux-gnu/libsnore* /artefacts/
cp -av /usr/local/lib/libtoxcore* /artefacts/

chmod a+rwx /artefacts/*

' > $_HOME_/"$system_to_build_for"/script/run.sh

    docker run -ti --rm \
      -v $_HOME_/"$system_to_build_for"/artefacts:/artefacts \
      -v $_HOME_/"$system_to_build_for"/script:/script \
      -v $_HOME_/"$system_to_build_for"/workspace:/workspace \
      --net=host \
     "qtox_push_002_ub18_hw_encoding" \
     /bin/sh -c "apk add bash >/dev/null 2>/dev/null; /bin/bash /script/run.sh"
     if [ $? -ne 0 ]; then
        echo "** ERROR **:$system_to_build_for_orig"
        exit 1
     else
        echo "--SUCCESS--:$system_to_build_for_orig"
     fi

done


