#! /bin/bash

_HOME2_=$(dirname $0)
export _HOME2_
_HOME_=$(cd $_HOME2_;pwd)
export _HOME_

echo $_HOME_
cd $_HOME_

cp -a ../.ci-scripts/build-qtox-linux.sh .

build_for='
nixos/nix
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

echo '
{ pkgs ? import <nixpkgs> { } }:
with pkgs;
let
  libtoxcore'"'"' = libtoxcore.overrideAttrs ({ ... }: {
    src = fetchurl {
      url =
        "https://github.com/TokTok/c-toxcore/releases/download/v0.2.18/c-toxcore-0.2.18.tar.gz";
      sha256 = "sha256-8pQFN5mIY1k+KLxqa19W8JZ19s2KKDJre8MbSDbAiUI=";
    };
  });

  toxext = stdenv.mkDerivation rec {
    pname = "toxext";
    version = "0.0.3";
    src = fetchFromGitHub {
      owner = pname;
      repo = pname;
      rev = "v${version}";
      hash = "sha256-I0Ay3XNf0sxDoPFBH8dx1ywzj96Vnkqzlu1xXsxmI1M=";
    };
    nativeBuildInputs = [ cmake pkg-config ];
    buildInputs = [ libtoxcore ];
  };

  toxextMessages = stdenv.mkDerivation rec {
    pname = "tox_extension_messages";
    version = "0.0.3";
    src = fetchFromGitHub {
      owner = "toxext";
      repo = pname;
      rev = "v${version}";
      hash = "sha256-qtjtEsvNcCArYW/Zyr1c0f4du7r/WJKNR96q7XLxeoA=";
    };
    nativeBuildInputs = [ cmake pkg-config ];
    buildInputs = [ libtoxcore toxext ];
  };

in pkgs.qtox.overrideAttrs ({ buildInputs, ... }: {
  version = "push_notification-";
  src =
    builtins.fetchGit "https://github.com/Zoxcore/qTox?ref=push_notification";
  buildInputs = buildInputs ++ [ curl libtoxcore'"'"' toxext toxextMessages ];
})

' > $_HOME_/"$system_to_build_for"/script/qtox.txt


    echo '#! /bin/bash

nix-build /script/qtox.txt

tar -czvf /artefacts/qtox_pushnotification_nixos.tar.gz /nix/store/*tox*

chmod a+rwx /artefacts/*

' > $_HOME_/"$system_to_build_for"/script/run.sh

    docker run -ti --rm \
      -v $_HOME_/"$system_to_build_for"/artefacts:/artefacts \
      -v $_HOME_/"$system_to_build_for"/script:/script \
      -v $_HOME_/"$system_to_build_for"/workspace:/workspace \
      --net=host \
     "nixos/nix" \
     /bin/sh -c "bash /script/run.sh"
     if [ $? -ne 0 ]; then
        echo "** ERROR **:$system_to_build_for_orig"
        exit 1
     else
        echo "--SUCCESS--:$system_to_build_for_orig"
     fi

done


