{ pkgs ? import <nixpkgs> {} }:

pkgs.rustPlatform.buildRustPackage {
  pname = "sapporo";
  version = "0.2.1";

  src = ./.;

  cargoLock.lockFile = ./Cargo.lock;

  postInstall = ''
    # Bundle sapporo library files (needed at runtime to find sapporo.hk and sapporo.js)
    mkdir -p $out/share/sapporo
    cp ${../sapporo}/sapporo.js $out/share/sapporo/sapporo.js
    mkdir -p $out/share/sapporo/sapporo
    cp ${../sapporo}/sapporo/sapporo.hk $out/share/sapporo/sapporo/sapporo.hk
  '';

  meta = with pkgs.lib; {
    description = "Web app toolkit for Hokkaido — build, run, and deploy WASM apps";
    longDescription = ''
      sapporo is a CLI tool for building web applications with the Hokkaido
      compiler. It compiles .hk files to WebAssembly, bundles the sapporo
      DOM library, and provides dev server integration.
    '';
    homepage = "https://github.com/jihoo/hokkaido";
    license = licenses.asl20;
    maintainers = [];
    mainProgram = "sapporo";
  };
}
