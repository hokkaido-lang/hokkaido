{ pkgs ? import <nixpkgs> {} }:

pkgs.rustPlatform.buildRustPackage {
  pname = "hokup";
  version = "0.1.0";

  src = ./.;

  cargoLock.lockFile = ./Cargo.lock;

  meta = with pkgs.lib; {
    description = "Hokkaido toolchain installer (like rustup for hokkaido)";
    longDescription = ''
      hokup is the official installer and updater for the Hokkaido
      toolchain. It downloads and installs the hokkaido compiler,
      hok-lsp language server, and otaru package manager.
    '';
    homepage = "https://github.com/jihoo/hokkaido";
    license = licenses.asl20;
    maintainers = [];
    mainProgram = "hokup";
  };
}
