{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
        flake-utils.url = "github:numtide/flake-utils";
    };
    outputs = 
    { flake-utils, nixpkgs, ... }:
        flake-utils.lib.eachDefaultSystem (system:
        let
            pkgs = import nixpkgs { inherit system; };
        in {

        devShells.default = pkgs.mkShell {
            nativeBuildInputs = with pkgs.buildPackages; [
                doxygen
            ];

            shellHook = /* bash */ ''
                git config core.hooksPath .hooks # Set up git hooks
            '';

        };
    });
}
