{ pkgs ? import <nixpkgs> {} }:

pkgs.rustPlatform.buildRustPackage {
  pname = "sapporo";
  version = "0.2.2";

  src = ./.;

  cargoLock.lockFile = ./Cargo.lock;

  # Copy sapporo library into the build tree so include_bytes! paths resolve
  # Source root is /build/sapporo-cli, include_bytes! expects ../sapporo/
  preBuild = ''
    mkdir -p ../sapporo/sapporo
    cp ${../sapporo/sapporo.js} ../sapporo/sapporo.js
    cp ${../sapporo/sapporo/sapporo.hk} ../sapporo/sapporo/sapporo.hk
    cp ${../sapporo/hk.mod} ../sapporo/hk.mod 2>/dev/null || touch ../sapporo/hk.mod
  '';

  meta = with pkgs.lib; {
    description = "Web app toolkit for Hokkaido — build, run, and deploy WASM apps";
    longDescription = ''
      sapporo is a CLI tool for building web applications with the Hokkaido
      compiler. It compiles .hk files to WebAssembly, bundles the sapporo
      DOM library, and provides dev server integration.
    '';
    homepage = "https://github.com/hokkaido-lang/hokkaido";
    license = licenses.asl20;
    maintainers = [];
    mainProgram = "sapporo";
  };
}
