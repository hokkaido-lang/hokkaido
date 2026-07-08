{ pkgs ? import <nixpkgs> {} }:

pkgs.rustPlatform.buildRustPackage {
  pname = "otaru";
  version = "0.1.0";

  src = ./.;

  cargoLock.lockFile = ./Cargo.lock;

  meta = with pkgs.lib; {
    description = "Hokkaido package manager and project manager";
    longDescription = ''
      otaru is a package manager and project scaffold for the
      Hokkaido compiler. It handles project creation, building,
      running, dependency management, and more.
    '';
    homepage = "https://github.com/jihoo/hokkaido";
    license = licenses.mit;
    maintainers = [];
    mainProgram = "otaru";
  };
}
