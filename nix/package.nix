{
  lib,
  stdenv,
  kernel,
}:
stdenv.mkDerivation {
  pname = "gxfp-linux-driver";
  version = "unstable";

  src = lib.sources.cleanSource ../.;

  nativeBuildInputs = kernel.moduleBuildDependencies;

  makeFlags = [
    "KDIR=${kernel.dev}/lib/modules/${kernel.modDirVersion}/build"
  ];

  installPhase = ''
    runHook preInstall

    mkdir -p $out/lib/modules/${kernel.modDirVersion}/extra
    install -m644 gxfp.ko $out/lib/modules/${kernel.modDirVersion}/extra/gxfp.ko

    runHook postInstall
  '';

  dontStrip = true;

  passthru = {
    inherit (kernel) modDirVersion;
  };

  meta = {
    description = "Kernel module for the GXFP5130 fingerprint sensor over eSPI";
    license = lib.licenses.gpl2;
    platforms = lib.platforms.linux;
  };
}
