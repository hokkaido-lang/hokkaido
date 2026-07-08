{
  description = "hokkaido — LLVM-based compiler with Rust cubical backend";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        hokkaido = pkgs.callPackage ./default.nix { };
        otaru = pkgs.callPackage ./otaru/default.nix { };
      in
      {
        packages = {
          default = hokkaido;
          otaru = otaru;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ hokkaido ];
          buildInputs = with pkgs; [
            cmake
            otaru
          ];
          shellHook = ''
            echo "--- Compiler Development Environment Loaded ---"
            echo "Using CMake and $(llvm-config --version)"
          '';
        };
      });
}
