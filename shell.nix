# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      git
      bazel_5
      # For bazel to use the correct python,
      # use --repo_env PYTHON_BIN_PATH=`which python3` (see README)
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

      zlib  # Used by some external Python module ?

      ncurses
    ];
   shellHook =
   ''
       export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${pkgs.zlib}/lib:${pkgs.stdenv.cc.cc.lib}/lib
   '';
}
