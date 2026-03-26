self: {
  lib,
  config,
  ...
}: let
  cfg = config.hardware.gxfp;
  gxfp-linux-driver = config.boot.kernelPackages.callPackage ./package.nix {};
in {
  options.hardware.gxfp.enable = lib.mkEnableOption "Kernel module for the GXFP5130 fingerprint sensor over eSPI";

  config = lib.mkIf cfg.enable {
    boot.extraModulePackages = [gxfp-linux-driver];
    boot.kernelModules = ["gxfp"];
  };
}
