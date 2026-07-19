{ pkgs ? import <nixpkgs> {}
, hokkaido ? null
}:

let
  hokkaido-bin = if hokkaido != null then hokkaido
    else pkgs.callPackage ../default.nix { };
in

pkgs.rustPlatform.buildRustPackage {
  pname = "otaru";
  version = "0.8.0";

  src = ./.;

  cargoLock.lockFile = ./Cargo.lock;

  buildInputs = [ hokkaido-bin ];

  postInstall = ''
    # Bundle the hokkaido compiler
    mkdir -p $out/bin
    cp ${hokkaido-bin}/bin/hokkaido $out/bin/
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
