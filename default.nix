{ pkgs ? import <nixpkgs> {} }:

pkgs.stdenv.mkDerivation {
  pname = "hokkaido";
  version = "1.0.0";

  src = ./.;

  nativeBuildInputs = with pkgs; [
    cmake
  ];

  buildInputs = with pkgs; [
    llvmPackages.llvm
    llvmPackages.libllvm
  ];

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp hokkaido      $out/bin/
    cp hok-lsp       $out/bin/
    runHook postInstall
  '';

  meta = with pkgs.lib; {
    description = "Hokkaido compiler (LLVM-based)";
    mainProgram = "hokkaido";
  };
}
