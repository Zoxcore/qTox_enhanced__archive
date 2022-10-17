#! /bin/bash

_HOME2_=$(dirname $0)
export _HOME2_
_HOME_=$(cd $_HOME2_;pwd)
export _HOME_

echo $_HOME_
cd $_HOME_

cp -a ../.ci-scripts/build-qtox-linux.sh .

build_for='
nixos
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
let
  overlay = final: prev:
    # a fixed point overay for overriding a package set

    with final; {
      # use the final result of the overlay for scope

      libtoxcore =
        # build a custom libtoxcore
        prev.libtoxcore.overrideAttrs ({ ... }: {
          src = fetchFromGitHub {
            owner = "TokTok";
            repo = "c-toxcore";
            rev = "e0b00d3e733148823e4b63d70f464e523ad62bac";
            fetchSubmodules = true;
            sha256 = "sha256-ofFoeC3gYAxknqATjqCF8um69kTTAaazs4yGouRe5Wc=";
          };
          patches = [
            (fetchpatch {
              url =
                "https://raw.githubusercontent.com/Zoxcore/qTox/zoxcore/push_notification/buildscripts/patches/msgv3_addon.patch";
              sha256 = "sha256-OvS9N5dT7PiyYI2bMNSSawbRksvUvXGaAQHVBv5KUY0=";
            })
            (fetchpatch {
              url =
                "https://raw.githubusercontent.com/Zoxcore/qTox/zoxcore/push_notification/buildscripts/patches/tc___capabilites.patch";
              sha256 = "sha256-bNDhROluR92rP6wgUDU6J7IWBjpIHcX7oZUfCKnU7No=";
            })
            (fetchpatch {
              url =
                "https://raw.githubusercontent.com/Zoxcore/qTox/zoxcore/push_notification/buildscripts/patches/add_tox_group_get_grouplist_function.patch";
              sha256 = "sha256-E8mNRwi07j2L8Vxg5n+A/taUE8pvY0m1MsIw+uW/+Cs=";
            })
          ];
        });

      toxext =
        # use an existing package or package it here
        prev.toxext or stdenv.mkDerivation rec {
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

      toxextMessages =
        # use an existing package or package it here
        prev.toxextMessages or stdenv.mkDerivation rec {
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

      qtox = prev.qtox.overrideAttrs ({ buildInputs, ... }: {
        version = "push_notification-";
        # take sources directly from this repo checkout
        buildInputs = buildInputs ++ [ curl libtoxcore toxext toxextMessages ];
      });

    };

in { pkgs ? import <nixpkgs> { } }:
# take nixpkgs from the environment
let
  pkgs'"'"' = pkgs.extend overlay;
  # apply overlay
in pkgs'"'"'.qtox
# select package

' > $_HOME_/"$system_to_build_for"/script/qtox.txt


    echo '#! /bin/bash

set -Eeuo pipefail
nix-build /script/qtox.txt

tar -czvf /artefacts/qtox_pushnotification_nixos.tar.gz /nix/store/*tox*

chmod a+rwx /artefacts/*

' > $_HOME_/"$system_to_build_for"/script/run.sh

    docker run -ti --rm \
      -v $_HOME_/"$system_to_build_for"/artefacts:/artefacts \
      -v $_HOME_/"$system_to_build_for"/script:/script \
      -v $_HOME_/"$system_to_build_for"/workspace:/workspace \
      --net=host \
     "nixos/nix:latest" \
     /bin/sh -c "bash /script/run.sh"
     if [ $? -ne 0 ]; then
        echo "** ERROR **:$system_to_build_for_orig"
        exit 1
     else
        echo "--SUCCESS--:$system_to_build_for_orig"
     fi

done


