#!/bin/bash
# Build static, decode-only FFmpeg and SDL2 for an older macOS deployment
# target (default: 10.15 Catalina).
#
# Homebrew bottles are compiled for the macOS release of the machine that built
# them, so a Receiver linked against them refuses to launch on older systems.
# Building the two dependencies from source with an explicit deployment target,
# and linking them statically, removes that floor.
#
# Usage:
#   MACOSX_DEPLOYMENT_TARGET=10.15 ./build_legacy_deps.sh /path/to/prefix
#
# Then build the Receiver against the prefix:
#   PKG_CONFIG_LIBDIR=/path/to/prefix/lib/pkgconfig PKG="pkgconf --static" \
#     MACOSX_DEPLOYMENT_TARGET=10.15 ./build_tbreceiver_c_app.sh
#
# Build tools needed: cmake, nasm, pkgconf (brew install cmake nasm pkgconf).
set -euo pipefail

DEPLOY="${MACOSX_DEPLOYMENT_TARGET:-10.15}"
PREFIX="${1:-$PWD/legacy-deps}"
ARCH="${ARCH:-$(uname -m)}"
FFMPEG_TAG="${FFMPEG_TAG:-n8.1.3}"
SDL2_TAG="${SDL2_TAG:-release-2.32.10}"
JOBS="$(sysctl -n hw.ncpu)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

export MACOSX_DEPLOYMENT_TARGET="$DEPLOY"
TARGET_FLAGS="-arch $ARCH -mmacosx-version-min=$DEPLOY"
# Fail the build if anything calls an API newer than the deployment target
# without an availability guard; such a call would crash at launch on Catalina.
AVAIL_FLAGS="-Werror=unguarded-availability-new"

mkdir -p "$PREFIX"
PREFIX="$(cd "$PREFIX" && pwd)"

echo "==> SDL2 ${SDL2_TAG} (macOS ${DEPLOY}, ${ARCH})"
git clone --quiet --depth 1 --branch "$SDL2_TAG" https://github.com/libsdl-org/SDL.git "$WORK/SDL"
cmake -S "$WORK/SDL" -B "$WORK/SDL/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOY" \
  -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
  -DCMAKE_C_FLAGS="$AVAIL_FLAGS" \
  -DSDL_SHARED=OFF \
  -DSDL_STATIC=ON \
  -DSDL_STATIC_PIC=ON \
  -DSDL_TEST=OFF \
  -DSDL_TESTS=OFF \
  -DSDL_LIBICONV=OFF
cmake --build "$WORK/SDL/build" --parallel "$JOBS"
cmake --install "$WORK/SDL/build"

echo "==> FFmpeg ${FFMPEG_TAG} (macOS ${DEPLOY}, ${ARCH})"
git clone --quiet --depth 1 --branch "$FFMPEG_TAG" https://github.com/FFmpeg/FFmpeg.git "$WORK/ffmpeg"
(
  cd "$WORK/ffmpeg"
  # --disable-autodetect keeps Homebrew libraries on the build machine from
  # leaking into the link; everything the Receiver needs is enabled explicitly.
  ./configure \
    --prefix="$PREFIX" \
    --cc=clang \
    --arch="$ARCH" \
    --enable-static \
    --disable-shared \
    --enable-pic \
    --disable-autodetect \
    --disable-everything \
    --disable-programs \
    --disable-doc \
    --disable-network \
    --disable-avdevice \
    --disable-avformat \
    --disable-avfilter \
    --disable-swresample \
    --enable-pthreads \
    --enable-videotoolbox \
    --enable-swscale \
    --enable-decoder=h264,hevc \
    --enable-parser=h264,hevc \
    --enable-hwaccel=h264_videotoolbox,hevc_videotoolbox \
    --extra-cflags="$TARGET_FLAGS $AVAIL_FLAGS" \
    --extra-ldflags="$TARGET_FLAGS"

  for flag in CONFIG_VIDEOTOOLBOX HAVE_PTHREADS CONFIG_H264_VIDEOTOOLBOX_HWACCEL CONFIG_HEVC_VIDEOTOOLBOX_HWACCEL; do
    if ! grep -q "#define ${flag} 1" config.h; then
      echo "FFmpeg configure did not enable ${flag}" >&2
      exit 1
    fi
  done

  make -j"$JOBS"
  make install
)

echo "Legacy dependencies installed in $PREFIX"
