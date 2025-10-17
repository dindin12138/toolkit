{
  description = "A Nix-flake-based C/C++ development environment";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-25.05";

  outputs =
    { self, nixpkgs, ... }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forEachSupportedSystem = f: nixpkgs.lib.genAttrs supportedSystems f;
    in
    {
      devShells = forEachSupportedSystem (
        system:
        let
          pkgs = import nixpkgs { inherit system; };

          llvm = pkgs.llvmPackages;

          commonPackages = with pkgs; [
            cmake
            ninja
            bear
            pkg-config
          ];

          clangShell = (pkgs.mkShell.override { stdenv = llvm.stdenv; }) {
            name = "CPP-Clang";
            buildInputs = [
              llvm.lldb
            ]
            ++ commonPackages;
            shellHook = ''
              echo "Entered Clang C++ development environment."
              exec fish
            '';
          };

          gccShell = pkgs.mkShell {
            name = "CPP-GCC";
            buildInputs = [
              pkgs.gcc
              pkgs.gdb
            ]
            ++ commonPackages;
            shellHook = ''
              echo "Entered GCC C++ development environment."
              exec fish
            '';
          };

        in
        {
          clang = clangShell;
          gcc = gccShell;
          default = clangShell;
        }
      );
    };
}
