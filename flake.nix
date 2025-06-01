{
  description = "actorpp";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
  inputs.flake-utils.url = "github:numtide/flake-utils";

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      rec {
        packages.actorpp = pkgs.stdenv.mkDerivation {
          name = "actorpp";
          src = ./.;
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
          ];

          nativeCheckInputs = [
            pkgs.python3
          ];

          preCheck = ''
            python $src/test/test_server.py --fork --pid-file test_server_pid
          '';

          postCheck = ''
            kill $(cat test_server_pid)
          '';

          doCheck = true;
        };
        packages.default = packages.actorpp;

        devShells.actorpp = packages.actorpp.overrideAttrs (attrs: {
          nativeBuildInputs = attrs.nativeBuildInputs ++ [
            pkgs.clang-tools
            pkgs.nixfmt-rfc-style
          ];
        });
        devShells.default = devShells.actorpp;
      }
    );
}
