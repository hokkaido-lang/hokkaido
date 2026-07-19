{ pkgs ? import <nixpkgs> {}
, hokkaido ? null
}:

let
  hokkaido-bin = if hokkaido != null then hokkaido
    else pkgs.callPackage ../default.nix { };

  sapporo-src = ../sapporo;
in

pkgs.rustPlatform.buildRustPackage {
  pname = "otaru";
  version = "0.8.1";

  src = ./.;

  cargoLock.lockFile = ./Cargo.lock;

  buildInputs = [ hokkaido-bin ];

  postInstall = ''
    # Bundle the hokkaido compiler
    mkdir -p $out/bin
    cp ${hokkaido-bin}/bin/hokkaido $out/bin/

    # Bundle the sapporo DOM library (sapporo.hk + sapporo.js)
    mkdir -p $out/share/otaru/sapporo
    cp ${sapporo-src}/sapporo/sapporo.hk $out/share/otaru/sapporo/sapporo.hk
    cp ${sapporo-src}/sapporo.js $out/share/otaru/sapporo/sapporo.js
  '';

  meta = with pkgs.lib; {
    description = "Hokkaido package manager and build tool";
    longDescription = ''
      otaru is a package manager, project scaffold, and build tool for the
      Hokkaido compiler. It handles project creation, building, running,
      dependency management, C/C++ builds, WASM compilation, and web app
      scaffolding with built-in DOM library and dev server.
    '';
    homepage = "https://github.com/jihoo/hokkaido";
    license = licenses.asl20;
    maintainers = [];
    mainProgram = "otaru";
  };
}
