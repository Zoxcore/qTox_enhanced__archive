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
  pkgs' = pkgs.extend overlay;
  # apply overlay
in pkgs'.qtox
# select package
