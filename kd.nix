# kd.nix — a package, not a flake. Build it with:
#
#   nix-build -E 'with import <nixpkgs> {}; callPackage ./kd.nix {}'
#
# This is the glibc build, which is what nixpkgs means by stdenv. Sorting
# follows LC_COLLATE here, not code point.

{ lib
, stdenv
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "kd";
  version = "0.1.6";

  # Only what the build reads. Screenshots and notes change often and would
  # otherwise cost a rebuild each time.
  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [ ./kd.c ./kf.h ./kd.1 ./Makefile ];
  };

  # The Makefile names clang; a command line assignment beats a file
  # assignment, and stdenv's cc is the one that knows this platform's libc.
  makeFlags = [
    "CC=${stdenv.cc.targetPrefix}cc"
    "PREFIX=${placeholder "out"}"
  ];

  enableParallelBuilding = true;

  doInstallCheck = stdenv.buildPlatform.canExecute stdenv.hostPlatform;
  installCheckPhase = ''
    $out/bin/kd . > /dev/null
    $out/bin/kd -l . > /dev/null
  '';

  # kd -G execs git from PATH on purpose, so it uses the git you use. Nothing
  # is wrapped here; wrapping would pin a git the user never asked for.

  meta = {
    description = "Directory lister in plain C, eza without the mess";
    homepage = "https://github.com/m8l8th814n-eng/kd";
    license = lib.licenses.mit;
    mainProgram = "kd";
    platforms = lib.platforms.unix;
  };
})
