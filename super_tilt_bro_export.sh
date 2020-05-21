#!/bin/bash

#
# Rebuild em-fceux with latest Super Tilt Bro. ROM
#
# Troubleshooting
# ---------------
#
# binaryen not found:
#
#  Symptom:
#    Fails with a log like that:
#      shared:ERROR: BINARYEN_ROOT is set to empty value in ...
#
#  Solution:
#    In file from the log, set /usr as default binaryen root:
#      BINARYEN_ROOT = os.path.expanduser(os.getenv('BINARYEN', '/usr'))
#
#  Details:
#    binaryen is a critical component of emscripten toolchain. It is a set of executables placed
#    in /usr/bin/wasm-* by archlinux package emscripten. This patch tells emscripten where to find
#    it by default. (Note: I failed to set it with the environment variable, so hack the config)
#
# llvm not installed:
#
#  Symptom:
#    Fails with a log like that:
#      shared:ERROR: llc executable not found at `/usr/lib/emscripten-fastcomp/llc`
#
#  Solution:
#    rm ~/.emscripten
#    pacman -S extra/llvm
#
#  Details:
#    It seems that we can install emscripten without installing llvm. It is still
#    needed to compile this project.
#
# bad version of llvm:
#
#  Symptom:
#    emcc outputs a warning at each invocation (even emcc -v):
#      shared:WARNING: LLVM version appears incorrect (seeing "10.0", expected "11.0")
#
#  Solution:
#    Ignore it, it seems to work.
#
#  Details:
#    It seems that for whatever reason, Archlinux's emscripten packages comes with a bad LLVM
#    version in /usr/lib/emscripten-llvm (hopefully will be fixed one day)
#

set -e
set -x

rm -rf \
	src/drivers/em/site/fceux.* \
	src/drivers/em/site/config.js \
	src/drivers/em/deploy/
cp ~/workspace/nes/tilt/Super_Tilt_Bro_\(E\).nes src/drivers/em/assets/data/games/Super\ Tilt\ Bro\(E\).nes
./build-emscripten.sh
./build-site.sh
rm -rf /tmp/super_tilt_bro
cp -r src/drivers/em/deploy/ /tmp/super_tilt_bro
