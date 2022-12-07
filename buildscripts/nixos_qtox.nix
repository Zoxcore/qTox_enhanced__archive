{ pkgs ? import <nixpkgs> { } }:
# take nixpkgs from the environment
let
  overlay = import ./overlay.nix;
  pkgs' = pkgs.extend overlay;
  # apply overlay
in pkgs'.qtox
# select package
