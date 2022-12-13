{
  description = "qTox development flake";

  outputs = { self, nixpkgs }:
    let
      inherit (nixpkgs) lib;
      intersectOverlay = builtins.intersectAttrs (self.overlay { } { });
    in {
      overlay = import ./buildscripts/overlay.nix;

      legacyPackages =
        lib.mapAttrs (system: { extend, ... }: extend self.overlay)
        nixpkgs.legacyPackages;

      packages = lib.mapAttrs
        (system: pkgs: intersectOverlay pkgs // { default = pkgs.qtox; })
        self.legacyPackages;
    };
}
