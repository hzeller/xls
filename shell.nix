# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.
# This is not officially supported by XLS team.

{ pkgs ? import <nixpkgs> {} }:
let
  #xls_used_stdenv = pkgs.stdenv;

  # Alternatively, use ccache stddev, so after bazel clean
  # it is much cheaper to rebuild the world.
  #
  # This requires that you add a line to your ~/.bazelrc
  # echo "build --sandbox_writable_path=$HOME/.cache/ccache" >> ~/.bazelrc
  # Works on nixos, but noticed issues with just nix package manager.
  xls_used_stdenv = pkgs.ccacheStdenv;

  # This does not work yet out of the box
  # https://github.com/NixOS/nixpkgs/issues/216047
  #xls_used_stdenv = pkgs.clang13Stdenv;
in
xls_used_stdenv.mkDerivation {
  name = "xls-build-environment";
  buildInputs = with pkgs;
    [
      bazel_5
      jdk11
      git cacert    # some recursive workspace dependencies via git.

      python310

      # Python packages needed found with
      #   find . -name BUILD | xargs grep -h "requirement(" | sort | uniq
      # Mentioned here, but looks like PIP does not use the system
      # packages yet. See below.
      python310Packages.click
      python310Packages.flask
      python310Packages.itsdangerous
      python310Packages.jinja2
      python310Packages.markupsafe
      python310Packages.numpy
      python310Packages.portpicker
      python310Packages.psutil
      python310Packages.pyyaml
      python310Packages.scipy
      python310Packages.setuptools
      python310Packages.termcolor
      python310Packages.werkzeug

      # Other dependencies needed in the build process.
      perl    # iverilog uses perl to create its config.h
      ncurses # provides tic
      zlib    # PIP introduces impurities and downloads code that needs zlib.

      # Development support
      lcov              # Coverage.
      bazel-buildtools  # buildifier
      clang-tools_14    # clang-format
    ];

   shellHook =
   ''
       # Due to the use of pip, some binary code is downloaded from the
       # Internet, making the build non-hermetic.
       # The downloaded code contains binary platform libraries that
       # assume they can link against local shared libraries. Allow this
       # here for now by making them visible to the dynamic linker,
       # but this needs to be addressed to only use hermetically
       # supplied packages from the system.
       export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${pkgs.zlib}/lib
       export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${pkgs.stdenv.cc.cc.lib}/lib
   '';
}
