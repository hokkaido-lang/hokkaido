{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {

  buildInputs = with pkgs; [
    python3
    llvmPackages.llvm
    llvmPackages.libllvm
  ];

  shellHook = ''
    echo "--- Compiler Development Environment Loaded ---"
    echo "Using CMake and $(llvm-config --version)"
  '';
}
