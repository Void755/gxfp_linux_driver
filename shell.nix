{pkgs ? import <nixpkgs> {}}: let
  kernel = pkgs.linuxPackages_latest.kernel;
in
  pkgs.mkShell {
    name = "gxfp-dev-shell";

    nativeBuildInputs = with pkgs; [
      kernel.dev
      gnumake
      linuxHeaders
      clang-tools
      bear
    ];

    KERNELDIR = "${kernel.dev}/lib/modules/${kernel.modDirVersion}/build";

    shellHook = ''
      echo "Target Kernel: ${kernel.modDirVersion}"
      echo "Running Kernel: $(uname -r)"
      echo "KERNELDIR: $KERNELDIR"
    '';
  }
