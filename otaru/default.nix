{ pkgs ? import <nixpkgs> {}
, hokkaido ? null
, std ? null
}:

let
  hokkaido-bin = if hokkaido != null then hokkaido
    else pkgs.callPackage ../default.nix { };
  std-src = if std != null then std else ../std;
in

pkgs.rustPlatform.buildRustPackage {
  pname = "otaru";
  version = "0.6.0";

  src = ./.;

  cargoLock.lockFile = ./Cargo.lock;

  buildInputs = [ hokkaido-bin ];

  postInstall = ''
    # Bundle the hokkaido compiler
    mkdir -p $out/bin
    cp ${hokkaido-bin}/bin/hokkaido $out/bin/

    # Bundle the std library
    mkdir -p $out/share/otaru
    cp -r ${std-src} $out/share/otaru/std
  '';

  meta = with pkgs.lib; {
    description = "Hokkaido package manager and project manager";
    longDescription = ''
      otaru is a package manager and project scaffold for the
      Hokkaido compiler. It handles project creation, building,
      running, dependency management, and more.
    '';
    homepage = "https://github.com/jihoo/hokkaido";
    license = licenses.asl20;
    maintainers = [];
    mainProgram = "otaru";
  };
}
