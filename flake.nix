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
            packages = with pkgs; [
                git
                wayland-scanner
            ];

            nativeBuildInputs = with pkgs.buildPackages; [
                clang
                pkg-config
                wayland
                vulkan-headers
                libxkbcommon
                libGL
                glfw
            ];

            env = {
                LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath (with pkgs; [
                    wayland
                    libGL
                    libxkbcommon
                    vulkan-loader
                ]);
            };


            shellHook = /* bash */ ''

                # Set up git hooks
                git config core.hooksPath .hooks

                export PYTHONPATH="$PYTHONPATH:$(pwd)/botplot/src/"
            '';

        };
    });
}
