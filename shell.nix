# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

# Use ccache stddev, so that a bazel clean makes it much
# cheaper to rebuild the world.
# However, this requires that you add the following line
# to your ~/.bazelrc
# common --sanbdox_writeable_path=/your/home/directory/.cache/ccache

{ pkgs ? import <nixpkgs> {} }:
pkgs.ccacheStdenv.mkDerivation {
  name = "ccache-enabled";
  buildInputs = with pkgs;
    [
      git cacert
      bazel_5
      # For bazel to use the correct python,
      # use --repo_env PYTHON_BIN_PATH=`command -v python3` (see README)
      python310
      jdk11

      # Python packages needed found with
      # find . -name BUILD | xargs grep -h "requirement(" | sort | uniq
      python310Packages.setuptools
      python310Packages.scipy
      python310Packages.numpy
      python310Packages.click
      python310Packages.portpicker
      python310Packages.flask
      python310Packages.itsdangerous
      python310Packages.jinja2
      python310Packages.markupsafe
      python310Packages.psutil
      python310Packages.pyyaml
      python310Packages.werkzeug
      python310Packages.termcolor

      perl  # iverilog uses perl to create its config.h

      zlib  # PIP introduces impurities and downloads code that needs zlib.

      # Development support
      bazel-buildtools  # buildifier
    ];
   shellHook =
   ''
       # Due to the use of pip, some binary code is downloaded from the
       # internet, introducing a non-hermetic build.
       # The downloaded code contains binary platform libraries that
       # assume they can link against local shared libraries. Allow this
       # here for now, but this needs to be addressed.
       export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${pkgs.zlib}/lib
       export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${pkgs.stdenv.cc.cc.lib}/lib
   '';
}
