{
  description = "gxfp kernel driver package for NixOS";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = {
    self,
    nixpkgs,
    ...
  }: let
    supportedSystems = ["x86_64-linux" "aarch64-linux"];
    forEachSystem = f:
      nixpkgs.lib.genAttrs supportedSystems (system:
        f (import nixpkgs {
          inherit system;
        }));
  in {
    overlays.default = final: _prev: {
      gxfp-linux-driver = final.callPackage ./nix/package.nix {};
    };

    packages = forEachSystem (pkgs: rec {
      gxfp-linux-driver = pkgs.callPackage ./nix/package.nix {};
      default = gxfp-linux-driver;
    });

    nixosModules = rec {
      gxfp-linux-driver = import ./nix/module.nix self;
      default = gxfp-linux-driver;
    };
  };
}
