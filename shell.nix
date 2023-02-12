# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

# Use ccache stddev, so that a bazel clean makes it much
# cheaper to rebuild the world.
#
# However, this requires that you add a line to your ~/.bazelrc
# echo "build --sandbox_writable_path=$HOME/.cache/ccache" >> ~/.bazelrc

{ pkgs ? import <nixpkgs> {} }:
pkgs.ccacheStdenv.mkDerivation {
  name = "ccache-enabled";
  buildInputs = with pkgs;
    [
      bazel_5
      jdk11
      git cacert    # some recursive workspace dependencies via git.

      # For bazel to use the correct python (see README), use
      # bazel --repo_env PYTHON_BIN_PATH=`command -v python3`
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

      perl  # iverilog uses perl to create its config.h

      zlib  # PIP introduces impurities and downloads code that needs zlib.

      # Development support
      bazel-buildtools  # buildifier
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
